#pragma org(0xE030)

__xram __at(0xE720) unsigned int results[8];
__xram __at(0xE740) unsigned int calls;

unsigned char choose(void)
{
    calls++;
    return 1;
}

unsigned int main(void)
{
    unsigned char selector = 1;
    unsigned char nested = 2;
    unsigned int word = 0x0204;
    unsigned int unmatched = 9;
    unsigned int index = 0;

    switch (choose()) {
    case 1:
        results[0] = 11;
    case 2:
        results[1] = 22;
        break;
    default:
        results[1] = 99;
    }
    results[2] = calls;

    switch (unmatched) {
    case 3:
        results[3] = 3;
        break;
    default:
        results[3] = 33;
    }

    switch (word) {
    case 0x0102:
        results[4] = 12;
        break;
    case 0x0204:
        results[4] = 44;
        break;
    default:
        results[4] = 99;
    }

    switch (selector) {
    case 1:
        switch (nested) {
        case 2:
            results[5] = 55;
            break;
        default:
            results[5] = 99;
        }
        break;
    default:
        results[5] = 12;
    }

    while (index < 3) {
        switch (index) {
        case 0:
            index++;
            continue;
        case 1:
            results[7] = 71;
            break;
        default:
            index = 3;
            break;
        }
        index++;
    }
    results[6] = index;

    return calls;
}
