#!/bin/bash -e

# Overwrite the check-admins function to return true
cat <<EOF > /home/cadr/lispm.init
(advise http:check-admins :around test-admin 0
		t)
EOF

# Start the service
/usr/bin/supervisord &
sleep 10
until curl -m 10 http://localhost:31337/debug | grep "user: CADR" ; do
    printf '.'
    sleep 10
done

sleep 10
PAYLOAD="%22hello%20world%22"
OUTPUT=$(curl -v -m 30 $(python3 -c "print('http://localhost:31337/\x08?cmd=$PAYLOAD')"))
echo "$OUTPUT"

(echo "$OUTPUT" | grep "hello world") || (echo "PUBLIC: The admin cannot access the admin functionality"; exit -1)
kill -9 $(jobs -p) || true
exit 0
