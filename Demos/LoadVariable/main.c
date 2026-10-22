#org 33000

word value at 32 = 1234;
word words[2] at 40 = (1234, 5678);
byte xram bytes[2] at 34000 = (12, 34);
word xram wordResult;
byte xram byteResult;

main()
{
	// Exercise global word, register word-array and fixed-address XRAM byte-array loads.
	wordResult = value;
	wordResult = words[1];
	byteResult = bytes[1];
}
