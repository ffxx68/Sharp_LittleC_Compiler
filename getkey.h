// PC-1403
// getkey.h - alias for key_wait() (blocking key wait)
// This file exists for compatibility with code using getkey()

#include key.h

// Alias: getkey() calls key_wait()
char getkey() {
    return key_wait();
}

