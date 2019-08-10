ARG SERVICE=build--this--with--a--build-arg
FROM $SERVICE

RUN apt-get install -q -y curl
copy check_admin_accessible.sh /
copy check_get_users.sh /
copy check_size_same.py /

CMD ["/bin/sleep", "2d"]
