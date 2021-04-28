//#define dispWire 0x27

void writeStr(const char* s) {
	int i = 0;
	while (s[i] != 0) { digoleSerial.write(s[i]);  i++; }

}

void write2B(unsigned int v) {

	if (v < 255) write(v);
	else {
		write(255);
		write(v - 255);
	}

}

size_t write(uint8_t value) {
	digoleSerial.write(value);
	return 1; // assume sucess
}

void gPrint(const char v[]) {
	writeStr("TT");
	writeStr(v);
	write((uint8_t)0);
}

void gPrint(char v) {
	writeStr("TT");
	write(v);
	write((uint8_t)0);
}

//void gPrint(uint16_t v) {
//	int2str(v, strVal, 4);
//	gPrint(strVal);
//}

void gPrint(int v) {
	int2str(v, strVal);
	gPrint(strVal);
}

void int2str(int i, char* buf) {

	byte b = 0;
	boolean zeros = false;
	int div = 1;

	if (i < 10) div = 1;
	else if ((i >= 10) && (i < 100)) div = 10;
	else if ((i >= 100) && (i < 1000)) div = 100;
	else if ((i >= 1000) && (i < 10000)) div = 1000;
	else if ((i >= 10000) && (i < 100000)) div = 10000;

	//for(int div=10, mod=0; div>0; div/=10){
	for (int mod = 0; div > 0; div /= 10) {
		mod = i % div;
		i /= div;
		if (!zeros || i != 0) { zeros = false; buf[b++] = i + '0'; }
		i = mod;
	}

	buf[b] = 0;
}

void clearDigoleScreen() {
	setBgColor(BLACK);
	writeStr("CL");
	writeStr("SD1");
	setColor(WHITE);
}

void setBgColor(uint8_t color) {
	gCommand("BGC", color);
}

void setColor(uint8_t color) {
	gCommand("SC", color);
}

void setFont(uint8_t font) {
	gCommand("SF", font);
}

void setFlashFont(unsigned long int addr) {
	writeStr("SFF");
	write(addr >> 16);
	write(addr >> 8);
	write(addr);
}


void setTextPosAbs(unsigned int x, unsigned int y) {
	gCommand("ETP", x, y);
}

void setPrintPos(unsigned int x, unsigned int y) {
	gCommand("TP", x, y);
}

void drawFrame(unsigned int x, unsigned int y, unsigned int w, unsigned int h) {
	gCommand("DR", x, y, x + w, y + h);
}

void drawBox(unsigned int x, unsigned int y, unsigned int w, unsigned int h) {
	gCommand("FR", x, y, x + w, y + h);
}

void drawLine(unsigned int x, unsigned int y, unsigned int x1, unsigned int y1) {
	gCommand("LN", x, y, x1, y1);
}

void drawLineTo(unsigned int x, unsigned int y) {
	gCommand("LT", x, y);
}

void drawCircle(unsigned int x, unsigned int y, unsigned int r) {
	gCommand("CC", x, y, r);
}

void gCommand(const char c[], uint8_t val) {
	int i = 0;
	while (c[i] != 0) digoleSerial.write(c[i++]);
	digoleSerial.write(val);
}

void gCommand(const char c[], unsigned int x, unsigned int y) {
	int i = 0;
	while (c[i] != 0) digoleSerial.write(c[i++]); // command

	if (x < 255) digoleSerial.write(x);
	else { digoleSerial.write(0xff);  digoleSerial.write(x - 255); } // x

	if (y < 255) digoleSerial.write(y);
	else { digoleSerial.write(0xff);  digoleSerial.write(y - 255); } // y
}

void gCommand(const char c[], unsigned int x, unsigned int y, unsigned int r) {
	int i = 0;
	while (c[i] != 0) digoleSerial.write(c[i++]); // command

	if (x < 255) digoleSerial.write(x);
	else { digoleSerial.write(0xff);  digoleSerial.write(x - 255); } // x

	if (y < 255) digoleSerial.write(y);
	else { digoleSerial.write(0xff);  digoleSerial.write(y - 255); } // y

	if (r < 255) digoleSerial.write(r);
	else { digoleSerial.write(0xff);  digoleSerial.write(r - 255); } // r

	digoleSerial.write((byte)0x00);
}

void gCommand(const char c[], unsigned int x, unsigned int y, unsigned int x2, unsigned int y2) {
	int i = 0;
	while (c[i] != 0) digoleSerial.write(c[i++]); // command

	if (x < 255) digoleSerial.write(x);
	else { digoleSerial.write(0xff);  digoleSerial.write(x - 255); } // x

	if (y < 255) digoleSerial.write(y);
	else { digoleSerial.write(0xff);  digoleSerial.write(y - 255); } // y

	if (x2 < 255) digoleSerial.write(x2);
	else { digoleSerial.write(0xff);  digoleSerial.write(x2 - 255); } // x2

	if (y2 < 255) digoleSerial.write(y2);
	else { digoleSerial.write(0xff);  digoleSerial.write(y2 - 255); } // x2

	digoleSerial.write((byte)0x00);
}
