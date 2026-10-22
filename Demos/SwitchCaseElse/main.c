#org 0x8000

byte selector at 10;
byte result at 11;

main()
{
    selector = 1;
    switch (selector) {
        1: result = 10;
        else: result = 0;
    }
}
