FROM ubuntu:22.04

RUN apt-get update && apt-get install -y \
    gcc \
    libssl-dev \
    openssl \
    && rm -rf /var/lib/apt/lists/*

RUN mkdir -p /etc/security
RUN mkdir -p /opt/functions/audio

# Bake in hardware public key
COPY certs/trusted.pub /etc/security/trusted.pub

# Bake in AES key (pre-shared for demo)
COPY certs/aes.key /etc/security/aes.key

COPY vecu_agent /usr/bin/vecu_agent
RUN chmod +x /usr/bin/vecu_agent

EXPOSE 9000

CMD ["/usr/bin/vecu_agent"]
