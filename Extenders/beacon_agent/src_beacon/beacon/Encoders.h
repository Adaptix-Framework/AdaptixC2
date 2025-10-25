#pragma once

char* b64_encode(const unsigned char* in, int len);

int b64_decoded_size(const char* in);

int b64_decode(const char* in, unsigned char* out, int outlen);
