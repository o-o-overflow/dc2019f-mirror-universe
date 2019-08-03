#!/usr/bin/env python3
import collections
import enum
import io
import logging
import os
import queue
import random
import socket
import stat
import struct
import threading
import time

l = logging.getLogger("chaoslib")

@enum.unique
class CHAOS_OP(enum.Enum):
    RFC = 1
    OPN = 2
    CLS = 3
    FWD = 4
    ANS = 5
    SNS = 6
    STS = 7
    RUT = 0o10
    LOS = 0o11
    LSN = 0o12
    MNT = 0o13
    EOF = 0o14
    UNC = 0o15
    BRD = 0o16
    DAT = 0o200
    DAT1 = 0o201
    DAT2 = 0o202

CHAOS_MAX_DATA_SIZE = 488

# return in the CADR is 0o215    
cadr_return = b"\x8d"

ChaosPacket = collections.namedtuple('ChaosPacket', ['op', 'data', 'dest_addr', 'dest_idx', 'src_addr', 'src_idx', 'pkt_num', 'ack', 'hw_dest_addr', 'hw_src_addr', 'hw_checksum'])

def replace_newlines_with_cadr(s):
    return s.replace(os.linesep.encode('utf-8'), cadr_return)

def replace_cadr_return_with_newlines(s):
    return s.replace(cadr_return, os.linesep.encode('utf-8'))

def my_conn_address():
    mypid = os.getpid()
    return f"/var/tmp/chaosd_{mypid}"

def test_http_rfc_packet():
    return b"\x00\x01\x04\x00\x01\x01\x00\x00\x06\x01\x00\x00\x44\x40\x46\x42\x45\x56\x41\x4c\x01\x01\x06\x01\x00\x00"

def add_chaosd_header(pkt):
    pkt_size = len(pkt)

    first_byte = (pkt_size >> 8) & 0xff
    second_byte = pkt_size & 0xff

    return bytes([first_byte, second_byte, 0, 0]) + pkt



def create_chaos_pkt(op, data, dest_addr, dest_idx, src_addr, src_idx, pkt_num, ack):

    assert isinstance(op, CHAOS_OP)
    
    data_size = len(data)

    # need to pad the data if it is an odd number
    if data_size % 2 == 1:
        data += b"X"

    header = struct.pack('<HHHHHHHH',
                         op.value << 8,
                         data_size,
                         dest_addr,
                         dest_idx,
                         src_addr,
                         src_idx,
                         pkt_num,
                         ack)

    # This is the hardware footer, and for now we assume that
    # src and dest are on the same subnet, and we set the checksum to 0
    # since it's not being checked anywhere
    footer = struct.pack('<HHH',
                         dest_addr,
                         src_addr,
                         0)

    return header + data + footer
                         

def chaosd_connect():
    fd = socket.socket(socket.AF_UNIX)

    my_addr = my_conn_address()
    
    if os.path.exists(my_addr):
        os.unlink(my_addr)
    
    fd.bind(my_addr)
    fd.connect("/var/tmp/chaosd_server")
    os.unlink(my_addr)

    return fd

class ChaosConnectionThread(threading.Thread):

    def __init__(self, src_addr, dest_addr, service, data_buffer):
        threading.Thread.__init__(self)
        self.fd = chaosd_connect()
        self.fd_lock = threading.RLock()

        self.my_addr = src_addr        
        self.my_pkt_num = 0
        
        self.dest_addr = dest_addr
        self.dest_idx = None
        self.dest_pkt_num = None
        
        self.service = service

        self.pending_packets = dict()
        
        self.data_buffer = data_buffer
        self.data_buffer_lock = threading.RLock()

        self.my_idx = random.randint(0, 65535)

        self.is_closed = True

        self.chaos_connection()
        self.is_closed = False

    def send_sts_ack(self):
        sts_pkt = create_chaos_pkt(CHAOS_OP.STS, struct.pack('<HH', self.dest_pkt_num, 0x40), self.dest_addr, self.dest_idx, self.my_addr, self.my_idx, self.my_pkt_num, self.dest_pkt_num)
        with self.fd_lock:
            self.fd.sendall(add_chaosd_header(sts_pkt))

    def close(self):
        if not self.is_closed:
            self.my_pkt_num = (self.my_pkt_num + 1) & 0xffff
            close_pkt = create_chaos_pkt(CHAOS_OP.CLS, b"Ending connection.", self.dest_addr, self.dest_idx, self.my_addr, self.my_idx, self.my_pkt_num, self.dest_pkt_num)
            with self.fd_lock:
                self.fd.sendall(add_chaosd_header(close_pkt))
                self.fd.close()
            self.is_closed = True

    def wait_for_sts_ack(self):
        while True:
            pkt = self.read_chaos_packet()
            l.debug(f"received {pkt}")
            if not pkt:
                l.warning("waiting for sts, but no more packets")
                return
            # this is for me
            if self.is_pkt_for_this_connection(pkt):
                l.info(f"received pkt for this connection {pkt}")
                if pkt.op == CHAOS_OP.STS:
                    # should be in response to what I just sent
                    receipt, window = struct.unpack('<HH', pkt.data)
                    if receipt == self.my_pkt_num:
                        l.info(f"got pkt I was expecting {receipt}")
                        break
                    else:
                        l.warning(f"got pkt I was NOT expecting {receipt}")
                self.process_received_packet(pkt)        

    def writeall(self, data):
        with self.fd_lock:
            # TODO: Chunk this into different packets of the correct sizes, max size is 488
            for i in range(0, len(data), CHAOS_MAX_DATA_SIZE):
                self.my_pkt_num = (self.my_pkt_num + 1) & 0xffff
                data_pkt = create_chaos_pkt(CHAOS_OP.DAT, data[i:i+CHAOS_MAX_DATA_SIZE], self.dest_addr, self.dest_idx, self.my_addr, self.my_idx, self.my_pkt_num, self.dest_pkt_num)
                self.fd.sendall(add_chaosd_header(data_pkt))
                self.wait_for_sts_ack()

            self.my_pkt_num = (self.my_pkt_num + 1) & 0xffff
            eof_pkt = create_chaos_pkt(CHAOS_OP.EOF, b"", self.dest_addr, self.dest_idx, self.my_addr, self.my_idx, self.my_pkt_num, self.dest_pkt_num)
            self.fd.sendall(add_chaosd_header(eof_pkt))
            self.wait_for_sts_ack()
            

    def is_pkt_for_this_connection(self, pkt):
        return pkt.hw_dest_addr == self.my_addr and pkt.dest_idx == self.my_idx

    def run(self):
        done = False
        while not done:
            try:
                pkt = self.read_chaos_packet()
            except socket.error:
                done = True
                continue
            if not pkt:
                l.warning(f"no pkts available, stopping.")
                return
            l.debug(f"pkt {pkt}")
            # this is for me
            if self.is_pkt_for_this_connection(pkt):
                self.process_received_packet(pkt)

    def process_received_packet(self, pkt):
        next_pkt_num = ((self.dest_pkt_num + 1) & 0xFFFF)
        if pkt.op == CHAOS_OP.CLS:
            self.close()
            with self.data_buffer_lock:
                self.data_buffer.put(None)

        elif pkt.op == CHAOS_OP.EOF:
            if pkt.pkt_num != next_pkt_num:
                l.info(f"received pkt {pkt.pkt_num} expecting {next_pkt_num}, queuing {pkt}")
                self.pending_packets[pkt.pkt_num] = pkt
                return
            # TODO: EOF, ack and close connection
            self.dest_pkt_num = pkt.pkt_num
            self.send_sts_ack()
            self.close()
            with self.data_buffer_lock:
                self.data_buffer.put(None)

        elif pkt.op == CHAOS_OP.SNS:
            self.send_sts_ack()
            
        elif pkt.op == CHAOS_OP.DAT:
            # got data, handle that
            # only accept if it is the next data packet,
            # we will be very simple for now and only accept the next data packet
            # (essentially having a window size of 1)
            if pkt.pkt_num != next_pkt_num:
                l.info(f"received pkt {pkt.pkt_num} expecting {next_pkt_num}, queuing {pkt}")
                self.pending_packets[pkt.pkt_num] = pkt
                return
            self.dest_pkt_num = pkt.pkt_num                
            self.send_sts_ack()
            with self.data_buffer_lock:
                for d in pkt.data:
                    self.data_buffer.put(d)
        else:
            # got some other packet, ignore it
            pass

        # check, do we have the next pending packet?
        next_pkt_num = ((self.dest_pkt_num + 1) & 0xFFFF)
        if next_pkt_num in self.pending_packets:
            next_pkt = self.pending_packets[next_pkt_num]
            l.info(f"we have the next pkt waiting {next_pkt_num}, let's handle {next_pkt}")
            del self.pending_packets[next_pkt_num]
            self.process_received_packet(next_pkt)
        

    def read_chaos_packet(self):
        # First, read the chaosd headers from the fd
        done = False
        while not done:
            time.sleep(0.1)
            with self.fd_lock:
                try:
                    self.fd.settimeout(0.1)
                    headers = self.fd.recv(4)
                except socket.timeout:
                    continue
                except OSError as e:
                    l.warning(f"exception {e} when trying to read, giving up")
                    return
                assert len(headers) == 4
                pkt_len = headers[0] << 8 | headers[1]
                raw_pkt = self.fd.recv(pkt_len)
                assert pkt_len == len(raw_pkt)
                done = True

        raw_op, data_size, dest_addr, dest_idx, src_addr, src_idx, pkt_num, ack = struct.unpack('<HHHHHHHH', raw_pkt[:16])
        op = CHAOS_OP(raw_op >> 8)

        data = raw_pkt[16:16+data_size]

        # if the data_size is odd then account for padding
        padding_offset = 0
        if data_size % 2 == 1:
            padding_offset = 1

        hw_dest_addr, hw_src_addr, hw_checksum = struct.unpack('<HHH', raw_pkt[16+data_size+padding_offset:])

        pkt = ChaosPacket(op, data, dest_addr, dest_idx, src_addr, src_idx, pkt_num, ack, hw_dest_addr, hw_src_addr, hw_checksum)
        return pkt
                

    def chaos_connection(self):
        # Send an RFC packet
        self.dest_idx = 0
        self.dest_pkt_num = 0

        # Wait for the response
        done = False
        send_rfc = True
        num_to_resend = 3
        i = 0
        while not done:
            if send_rfc or (i % num_to_resend) == 0:
                rfc_pkt = create_chaos_pkt(CHAOS_OP.RFC, self.service, self.dest_addr, self.dest_idx, self.my_addr, self.my_idx, self.my_pkt_num, self.dest_pkt_num)
                with self.fd_lock:
                    self.fd.sendall(add_chaosd_header(rfc_pkt))
                send_rfc = False
            i += 1
            pkt = self.read_chaos_packet()
            l.debug(f"pkt {pkt}")
            if not pkt:
                raise Exception("unable to establish a connection.")
            # this is for me
            if self.is_pkt_for_this_connection(pkt):
                l.info("received pkt for this connection {pkt}")
                if pkt.op == CHAOS_OP.CLS:
                    raise Exception("unable to establish a connection.")
                elif pkt.op == CHAOS_OP.ANS:
                    raise Exception("Currently unable to handle a simple answer.")
                elif pkt.op != CHAOS_OP.OPN:
                    raise Exception(f"Unable to handle response op {pkt.op}.")

                # data of the packet should be the window size, I suppose we'll ignore it for now
                self.dest_idx = pkt.src_idx
                self.dest_pkt_num = pkt.pkt_num
                l.info(f"dest index {self.dest_idx} dest pkt num {self.dest_pkt_num}")

                # respond to complete the connection
                self.send_sts_ack()
                break
    

class ChaosStream(io.RawIOBase):

    def __init__(self, src_addr, dest_addr, service):

        self._data_buffer = queue.Queue()

        self.connection_thread = ChaosConnectionThread(src_addr, dest_addr, service, self._data_buffer)

        self.connection_thread.start()

    def writeall(self, data):
        self.connection_thread.writeall(data)

    def read(self, n):
        if n == -1:
            return self.readall()
        assert n >= 0
        
        to_return = bytearray()
        for _ in range(n):
            b = self._data_buffer.get()

            # None is used to signify end of file
            if b == None:
                break
            to_return.append(b)
        return bytes(to_return)

    def readall(self):
        to_return = bytearray()
        while True:
            b = self._data_buffer.get()
            
            # None is used to signify EOF
            if b == None:
                break
            to_return.append(b)
        return bytes(to_return)

    def close(self):
        self.connection_thread.close()
    
        







