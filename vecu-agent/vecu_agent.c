#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <openssl/evp.h>
#include <openssl/pem.h>

#define PORT 9000
#define BUFFER_SIZE 65536
#define HARDWARE_KEY "/etc/security/trusted.pub"
#define AES_KEY_FILE "/etc/security/aes.key"
#define FLASH_PATH "/opt/functions/audio/audio.bin"

int aes_decrypt(const char *in_path, const char *out_path) {
    char cmd[512];
    snprintf(cmd, sizeof(cmd),
        "openssl enc -aes-256-cbc -pbkdf2 -d -in %s -out %s -pass file:%s",
        in_path, out_path, AES_KEY_FILE);
    int ret = system(cmd);
    return ret == 0;
}

int verify_signature(const char *fw_path, const char *sig_path) {
    FILE *keyfile = fopen(HARDWARE_KEY, "r");
    if (!keyfile) { printf("Hardware key not found\n"); return 0; }
    EVP_PKEY *pubkey = PEM_read_PUBKEY(keyfile, NULL, NULL, NULL);
    fclose(keyfile);
    if (!pubkey) { printf("Failed to load public key\n"); return 0; }

    FILE *fw = fopen(fw_path, "rb");
    fseek(fw, 0, SEEK_END);
    long fw_len = ftell(fw);
    rewind(fw);
    unsigned char *fw_data = malloc(fw_len);
    fread(fw_data, 1, fw_len, fw);
    fclose(fw);

    FILE *sf = fopen(sig_path, "rb");
    fseek(sf, 0, SEEK_END);
    long sig_len = ftell(sf);
    rewind(sf);
    unsigned char *sig_data = malloc(sig_len);
    fread(sig_data, 1, sig_len, sf);
    fclose(sf);

    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    EVP_DigestVerifyInit(ctx, NULL, EVP_sha256(), NULL, pubkey);
    EVP_DigestVerifyUpdate(ctx, fw_data, fw_len);
    int result = EVP_DigestVerifyFinal(ctx, sig_data, sig_len);

    EVP_MD_CTX_free(ctx);
    EVP_PKEY_free(pubkey);
    free(fw_data);
    free(sig_data);
    return result == 1;
}

void flash_firmware(const char *fw_path) {
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "cp %s %s", fw_path, FLASH_PATH);
    system(cmd);
    printf("Flashed to %s\n", FLASH_PATH);
}

int main() {
    int server_fd, client_fd;
    struct sockaddr_in addr;
    char buffer[BUFFER_SIZE];

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(PORT);

    bind(server_fd, (struct sockaddr*)&addr, sizeof(addr));
    listen(server_fd, 1);

    printf("vECU Verification Agent running on port %d\n", PORT);
    printf("/opt/functions/audio/ -- EMPTY, waiting for secure flash\n\n");

    while (1) {
        client_fd = accept(server_fd, NULL, NULL);
        printf("Flash tool connected\n");

        int fw_size = recv(client_fd, buffer, BUFFER_SIZE, 0);
        FILE *f = fopen("/tmp/received.enc", "wb");
        fwrite(buffer, 1, fw_size, f);
        fclose(f);
        printf("Encrypted firmware received (%d bytes)\n", fw_size);

        send(client_fd, "READY", 5, 0);

        int sig_size = recv(client_fd, buffer, BUFFER_SIZE, 0);
        f = fopen("/tmp/received.sig.enc", "wb");
        fwrite(buffer, 1, sig_size, f);
        fclose(f);
        printf("Encrypted signature received (%d bytes)\n", sig_size);

        printf("Decrypting firmware (AES-256)...\n");
        if (!aes_decrypt("/tmp/received.enc", "/tmp/received.bin")) {
            printf("Decryption failed -- rejected\n");
            send(client_fd, "FLASH_REJECTED", 14, 0);
            close(client_fd);
            continue;
        }

        printf("Decrypting signature...\n");
        if (!aes_decrypt("/tmp/received.sig.enc", "/tmp/received.sig")) {
            printf("Signature decryption failed -- rejected\n");
            send(client_fd, "FLASH_REJECTED", 14, 0);
            close(client_fd);
            continue;
        }

        printf("Verifying signature against hardware key...\n");
        if (verify_signature("/tmp/received.bin", "/tmp/received.sig")) {
            printf("VERIFIED -- flashing firmware\n");
            flash_firmware("/tmp/received.bin");
            send(client_fd, "FLASH_OK", 8, 0);
        } else {
            printf("INVALID signature -- rejected\n");
            send(client_fd, "FLASH_REJECTED", 14, 0);
        }

        close(client_fd);
        printf("\nWaiting for next flash request...\n");
    }

    return 0;
}
