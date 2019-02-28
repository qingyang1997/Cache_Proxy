FROM ubuntu:18.04
RUN mkdir /var/log/erss
RUN apt-get update && apt-get -y install g++ make
ADD ./logs /var/log/erss
ADD proxy.log /var/log/erss
WORKDIR /var/log/erss/logs

CMD ["./baby_proxy"]