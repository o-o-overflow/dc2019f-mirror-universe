// lmfs --- crude hack to manage Symbolics LMFS partitions

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>

#include "misc.h"

static int do_dir;
static int do_read;
static int do_write;

static char *img_filename = "FILE";
static char *path;

struct baccess {
	unsigned char buffer[8192];

	unsigned char *base;
	int offset;
	int record_no;
	int fd;
};

static void
init_access(struct baccess *pb, int fd)
{
	pb->base = pb->buffer;
	pb->offset = 0;
	pb->record_no = -1;
	pb->fd = fd;
}

static void *
ensure_access(struct baccess *pb, int offset, int size)
{
	unsigned char *ptr;
	int blknum;
	int boff;
	int left;

	blknum = offset / 1008;
	boff = offset % 1008;
	left = 1008 - boff;

	if (blknum > 3) {
		printf("ensure_access: past end!\n");
		exit(1);
	}

	if (left < size) {
		blknum++;
		boff = 0;
	}

	ptr = pb->base + (1024 * blknum) + 8 + boff;

	pb->offset = offset;

	return ptr;
}

static void *
advance_access(struct baccess *pb, int size)
{
	unsigned char *ptr;
	int blknum;
	int boff;
	int left;

	pb->offset += size;

	blknum = pb->offset / 1008;
	boff = pb->offset % 1008;
	left = 1008 - boff;

	if (blknum > 3) {
		return 0;
	}

	printf("left %d, size %d\n", left, size);
	if (left < size) {
		blknum++;
		boff = 0;
		pb->offset = blknum * 1008;

		if (blknum == 4) {
			return 0;
		}
	}

	ptr = pb->base + (1024 * blknum) + 8 + boff;

	printf("offset %d\n", pb->offset);

	return ptr;
}

static int
remaining_access(struct baccess *pb)
{
	int boff;
	int left;

	boff = pb->offset % 1008;
	left = 1008 - boff;
	return left;
}

static int
remaining_buffer(struct baccess *pb)
{
	return (1008 * 4) - pb->offset;
}

typedef int fixnum;
typedef int address;
typedef int date;
typedef char flag;

struct tapeinfo {
	date date;
	char tapename[8];
};

struct fh_element {
	char name[4];
	short location;		// In words.
	short length;		// In words.
};

struct partition_label {
	fixnum version;
	short name_len;
	char name[30];
	fixnum label_size;
	fixnum partition_id;
	struct {
		address primary_root;
		address free_store_info;
		address bad_track_info;
		address volume_table;
		address aspare[4];
	} disk_address;

	struct {
		date label_accepted;
		date shut_down;
		date free_map_reconstructed;
		date structure_salvaged;
		date scavenged;
		date tspare[4];
	} update_times;

	fixnum uid_generator;
	fixnum monotone_generator;
	struct fh_element root_list;
};

struct link_transparencies {
	struct {
		flag read_thru;
		flag write_thru;
		flag create_thru;
		flag rename_thru;
		flag delete_thru;
		flag lpad[6];
		int ignore;
	} attributes;
};

struct file_map {
	fixnum allocated_length;
	short valid_length;
	short link;
	fixnum element[1];
};

struct dir_header {
	short version;
	short size;
	short name_len;
	char name[30];

	short number_of_entries;
	short free_entry_list;
	short entries_index_offset;
	short direntry_size;
	short entries_per_block;
	short default_generation_retention_count;
	short uid_path_offset;
	short uid_path_length;
	short hierarch_depth;
	fixnum default_volid;
	flag auto_expunge_p;
	date auto_expunge_interval;
	date auto_expunge_last_time;

	struct link_transparencies default_link_transparencies;
};

struct directory_entry {
	short file_name_len;
	char file_name[30];
	short file_type_len;
	char file_type[14];
	char file_version[3];
	char bytesize;
	short author_len;
	char author[14];
	short file_name_true_length;
	fixnum number_of_records;

	struct {
		date date_time_created;
		date date_time_deleted;
		date date_time_reconstructed;
		date date_time_modified;
		date date_time_used;
	} dates;

	fixnum unique_ID;
	fixnum byte_length;
	char generation_retention_count;
	char partition_index[3];
	address record_0_address;

	struct tapeinfo archive_a;
	struct tapeinfo archive_b;
	short ignore_mode_word;
};

struct file_header {
	fixnum version;
	fixnum logical_size;
	fixnum bootload_generation;
	fixnum version_in_bootload;
	short number_of_elements;
	short ignore_mode_word;
	struct fh_element parts_array[8];
};

static int
read_record(struct baccess *pb, int record_no)
{
	int i;
	int block_no;
	unsigned char *pbuf;

	block_no = record_no * 4;
	pbuf = pb->buffer;
	pb->record_no = record_no;
	pb->offset = 0;

	for (i = 0; i < 4; i++) {
		read_block(pb->fd, block_no, pbuf);
		pbuf += BLOCKSZ;
		block_no++;
	}

	return 0;
}

#define STANDARD_BLOCK_SIZE 256
#define STANDARD_BLOCKS_PER_RECORD 4

static int
show_file(int fd, struct directory_entry *de, int record_no)
{
	struct baccess b;
	int i;
	int woffset;
	int wlast;
	int bl;
	int tot;
	struct file_header *fh;
	int n;
	int blocks[64];
	char *p;

	init_access(&b, fd);
	read_record(&b, record_no);

	fh = (struct file_header *) ensure_access(&b, 0, sizeof(struct file_header));

	printf("logical_size %d\n", fh->logical_size);
	printf("number_of_elements %d\n", fh->number_of_elements);
	for (i = 0; i < 8; i++) {
		char n[5];
		memcpy(n, fh->parts_array[i].name, 4);
		n[4] = 0;
		printf("%d: %s loc %d len %d\n", i, n, fh->parts_array[i].location, fh->parts_array[i].length);
	}
	for (i = 0; i < 8; i++) {
		woffset = fh->parts_array[i].location;
		wlast = fh->parts_array[i].location;

		if (memcmp(fh->parts_array[i].name, "fmap", 4) == 0) {
			struct file_map *fm;
			int j;
			fm = (struct file_map *) ensure_access(&b, woffset * 4, sizeof(struct file_map));
			printf("allocated_length %d\n", fm->allocated_length);
			printf("valid_length %d\n", fm->valid_length);
			printf("link %d\n", fm->link);
			printf("element[0] %d\n", fm->element[0]);
			for (j = 0; j < fm->valid_length; j++)
				blocks[j] = fm->element[j];
		}
	}

	bl = de->byte_length;
	printf("wlast %d\n", wlast);
	p = (char *) ensure_access(&b, wlast * 4, 0);
	tot = 0;

	int fdd;
	{
		char nn[1024];

		sprintf(nn, "tmp/%s", de->file_name);
		unlink(nn);
		fdd = open(nn, O_RDWR | O_TRUNC | O_CREAT, 0666);
	}

	n = 0;
	while (1) {
		int left;
		int use;

		left = remaining_access(&b);
		use = bl < left ? bl : left;
		tot += use;

		if (fdd > 0)
			write(fdd, p, use);

		bl -= use;
		p = (char *) advance_access(&b, use);

		if (remaining_buffer(&b) == 0) {
			printf("read record %d\n", blocks[n]);
			read_record(&b, blocks[n++]);
			p = (char *) ensure_access(&b, 0, 0);
		}

		if (bl <= 0)
			break;
	}

	printf("done\n");
	if (fdd > 0)
		close(fdd);

	return 0;
}

static int
show_de(int fd, int record_no)
{
	struct baccess b;
	struct directory_entry *de;
	int i;
	int woffset;
	int wlast;
	int numentries;
	struct file_header *fh;
	int n;
	int blocks[64];

	init_access(&b, fd);
	read_record(&b, record_no);

	printf("%d\n", record_no);

	fh = (struct file_header *) ensure_access(&b, 0, sizeof(struct file_header));
	printf("number_of_elements %d\n", fh->number_of_elements);
	for (i = 0; i < 8; i++) {
		char n[5];

		memcpy(n, fh->parts_array[i].name, 4);
		n[4] = 0;
		printf("%d: %s loc %d len %d\n", i, n, fh->parts_array[i].location, fh->parts_array[i].length);
	}

	for (i = 0; i < 8; i++) {
		woffset = fh->parts_array[i].location;
		wlast = fh->parts_array[i].location;

		if (memcmp(fh->parts_array[i].name, "fmap", 4) == 0) {
			struct file_map *fm;
			int j;

			fm = (struct file_map *) ensure_access(&b, woffset * 4, sizeof(struct file_map));
			printf("allocated_length %d\n", fm->allocated_length);
			printf("valid_length %d\n", fm->valid_length);
			printf("link %d\n", fm->link);
			printf("element[0] %d\n", fm->element[0]);
			for (j = 0; j < fm->valid_length; j++)
				blocks[j] = fm->element[j];
		}

		if (memcmp(fh->parts_array[i].name, "dire", 4) == 0) {
			struct directory_entry *de;

			de = (struct directory_entry *) ensure_access(&b, woffset * 4, sizeof(struct directory_entry));
			printf("file_name '%s', file_type '%s', bytesize %d, author '%s'\n", de->file_name, de->file_type, de->bytesize, de->author);
			printf("byte_length %d\n", de->byte_length);
			printf("number_of_records %d\n", de->number_of_records);
			printf("record_0_address %d\n", de->record_0_address);
		}
	}

	struct dir_header *dh;

	woffset = wlast;
	dh = (struct dir_header *) ensure_access(&b, woffset * 4, sizeof(struct dir_header));
	printf("version %d, size %d, name '%s'\n", dh->version, dh->size, dh->name);
	printf("number_of_entries %d\n", dh->number_of_entries);
	printf("entries_index_offset %d\n", dh->entries_index_offset);
	printf("uid_path_offset %d\n", dh->uid_path_offset);
	printf("uid_path_length %d\n", dh->uid_path_length);

	numentries = dh->number_of_entries;
	n = 0;
	for (i = 0; i < numentries; i++) {
		de = (struct directory_entry *) advance_access(&b, sizeof(struct directory_entry));
		if (de == 0) {
			read_record(&b, blocks[n++]);
			de = (struct directory_entry *) ensure_access(&b, 0, sizeof(struct directory_entry));
		}

		printf("#%d:\n", i + 1);
		dumpmem((char *) de, 16);
		printf("\n");
		printf("file_name '%s', file_type '%s', bytesize %d, author '%s'\n", de->file_name, de->file_type, de->bytesize, de->author);
		printf("byte_length %d\n", de->byte_length);
		printf("number_of_records %d\n", de->number_of_records);
		printf("record_0_address %d\n", de->record_0_address);
		printf("size %ld\n", sizeof(struct directory_entry));
		show_file(fd, de, de->record_0_address);
	}

	return 0;
}

static int
lmfs_open(char *img_filename, int offset)
{
	int fd;
	int ret;
	unsigned char buffer[BLOCKSZ];
	struct partition_label *pl;
	struct baccess b;

	fd = open(img_filename, O_RDONLY, 0666);
	if (fd < 0) {
		perror(img_filename);
		return -1;
	}

	ret = read(fd, buffer, BLOCKSZ);
	if (ret != BLOCKSZ) {
		perror(img_filename);
		return -1;
	}

	pl = (struct partition_label *) buffer;
	printf("version %d\n", pl->version);
	printf("name %s\n", pl->name);
	printf("primary_root %d\n", pl->disk_address.primary_root);
	printf("free_store_info %d\n", pl->disk_address.free_store_info);
	printf("bad_track_info %d\n", pl->disk_address.bad_track_info);
	printf("volume_table %d\n", pl->disk_address.volume_table);
	printf("\n");

	init_access(&b, fd);
	read_record(&b, pl->disk_address.primary_root);

	{
		struct file_header *fh;
		int i;
		int woffset;
		int wlast;

		fh = (struct file_header *) ensure_access(&b, 0, sizeof(struct file_header));
		printf("number_of_elements %d\n", fh->number_of_elements);

		for (i = 0; i < 8; i++) {
			char n[5];

			memcpy(n, fh->parts_array[i].name, 4);
			n[4] = 0;
			printf("%d: %s loc %d len %d\n", i, n, fh->parts_array[i].location, fh->parts_array[i].length);
			wlast = fh->parts_array[i].location;
		}

		for (i = 0; i < 8; i++) {
			woffset = fh->parts_array[i].location;

			if (memcmp(fh->parts_array[i].name, "fmap", 4) == 0) {
				struct file_map *fm;

				fm = (struct file_map *) ensure_access(&b, woffset * 4, sizeof(struct file_map));
				printf("allocated_length %d\n", fm->allocated_length);
				printf("valid_length %d\n", fm->valid_length);
				printf("link %d\n", fm->link);
				printf("element[0] %d\n", fm->element[0]);
			}
		}

		for (i = 0; i < 8; i++) {
			woffset = fh->parts_array[i].location;

			if (memcmp(fh->parts_array[i].name, "dire", 4) == 0) {
				struct directory_entry *de;
				int i;

				de = (struct directory_entry *)
					ensure_access(&b, woffset * 4, sizeof(struct directory_entry));

				printf("file_name '%s', file_type '%s', bytesize %d, author '%s'\n", de->file_name, de->file_type, de->bytesize, de->author);
				printf("number_of_records %d\n", de->number_of_records);
				printf("record_0_address %d\n", de->record_0_address);

				struct dir_header *dh;
				woffset = wlast;
				dh = (struct dir_header *) ensure_access(&b, woffset * 4, sizeof(struct dir_header));

				printf("version %d, size %d, name '%s'\n", dh->version, dh->size, dh->name);
				printf("number_of_entries %d\n", dh->number_of_entries);
				printf("entries_index_offset %d\n", dh->entries_index_offset);
				printf("uid_path_offset %d\n", dh->uid_path_offset);
				printf("uid_path_length %d\n", dh->uid_path_length);
				for (i = 0; i < dh->number_of_entries; i++) {
					de = (struct directory_entry *)
						advance_access(&b, sizeof(struct directory_entry));
					printf("file_name '%s', file_type '%s', bytesize %d, author '%s'\n", de->file_name, de->file_type, de->bytesize, de->author);
					printf("number_of_records %d\n", de->number_of_records);
					printf("record_0_address %d\n", de->record_0_address);
					printf("size %ld\n", sizeof(struct directory_entry));
					show_de(fd, de->record_0_address);
				}
			}
		}
	}

	close(fd);

	return 0;
}

static int
lmfs_show_dir(char *path)
{
	return 0;
}

static int
lmfs_read_file(char *path)
{
	return 0;
}

static int
lmfs_write_file(char *path)
{
	return 0;
}

static void
usage(void)
{
	fprintf(stderr, "usage: lmfs [OPTION]... [LMFS-FILE}\n");
	fprintf(stderr, "LMFS file extract\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "  -d DIR         show files in directory\n");
	fprintf(stderr, "  -r FILE        read file\n");
	fprintf(stderr, "  -w FILE        write file\n");
	fprintf(stderr, "  -h             help message\n");
}

int
main(int argc, char *argv[])
{
	int c;

	while ((c = getopt(argc, argv, "d:r:w:h")) != -1) {
		switch (c) {
		case 'd':
			do_dir++;
			path = strdup(optarg);
			break;
		case 'r':
			do_read++;
			path = strdup(optarg);
			break;
		case 'w':
			do_write++;
			path = strdup(optarg);
			break;
		case 'h':
			usage();
			exit(0);
		default:
			usage();
			exit(1);
		}
	}

	argc -= optind;
	argv += optind;

	if (argc > 0)
		img_filename = strdup(argv[0]);

	if (argc > 1) {
		usage();
		exit(1);
	}

	lmfs_open(img_filename, 0);

	if (do_dir) {
		lmfs_show_dir("");
	} else if (do_read) {
		lmfs_read_file(path);
	} if (do_write) {
		lmfs_write_file(path);
	}

	exit(0);
}
