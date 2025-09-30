// pasm_constants.h
// File header con tutte le costanti e array copiati dal Pascal originale
// NON MODIFICARE QUESTO FILE - Le costanti sono copiate esattamente dal pasm.dpr

#ifndef PASM_CONSTANTS_H
#define PASM_CONSTANTS_H

// Costanti opcode
#define NOP 77
#define JRNZP 40
#define JRNZM 41
#define JRNCP 42
#define JRNCM 43
#define JRP 44
#define JRM 45
#define LOOP 47
#define JRZP 56
#define JRZM 57
#define JRCP 58
#define JRCM 59

// Array di opcode (OPCODE) copiato dal Pascal originale
const char *OPCODE[256] = {
    "LII","LIJ","LIA","LIB","IX",
    "DX","IY","DY","MVW","EXW",
    "MVB","EXB","ADN","SBN","ADW",
    "SBW","LIDP","LIDL","LIP","LIQ",
    "ADB","SBB","?022?","?023?","MVWD",
    "EXWD","MVBD","EXBD","SRW","SLW",
    "FILM","FILD","LDP","LDQ","LDR",
    "RA","IXL","DXL","IYS","DYS",
    "JRNZP","JRNZM","JRNCP","JRNCM","JRP",
    "JRM","?046?","LOOP","STP","STQ",
    "STR","?051?","PUSH","DATA","?054?",
    "RTN","JRZP","JRZM","JRCP","JRCM",
    "?060?","?061?","?062?","?063?","INCI",
    "DECI","INCA","DECA","ADM","SBM",
    "ANMA","ORMA","INCK","DECK","INCM",
    "DECM","INA","NOPW","WAIT","WAITI",
    "INCP","DECP","STD","MVDM","READM",
    "MVMD","READ","LDD","SWP","LDM",
    "SL","POP","?092?","OUTA","?094?",
    "OUTF","ANIM","ORIM","TSIM","CPIM",
    "ANIA","ORIA","TSIA","CPIA","?104?",
    "DTJ","?106?","TEST","?108?","?109?",
    "?110?","?111?","ADIM","SBIM","?114?",
    "?115?","ADIA","SBIA","?118?","?119?",
    "CALL","JP","PTJ","?123?","JPNZ",
    "JPNC","JPZ","JPC","LP00","LP01",
    "LP02","LP03","LP04","LP05","LP06",
    "LP07","LP08","LP09","LP10","LP11",
    "LP12","LP13","LP14","LP15","LP16",
    "LP17","LP18","LP19","LP20","LP21",
    "LP22","LP23","LP24","LP25","LP26",
    "LP27","LP28","LP29","LP30","LP31",
    "LP32","LP33","LP34","LP35","LP36",
    "LP37","LP38","LP39","LP40","LP41",
    "LP42","LP43","LP44","LP45","LP46",
    "LP47","LP48","LP49","LP50","LP51",
    "LP52","LP53","LP54","LP55","LP56",
    "LP57","LP58","LP59","LP60","LP61",
    "LP62","LP63","INCJ","DECJ","INCB",
    "DECB","ADCM","SBCM","TSMA","CPMA",
    "INCL","DECL","INCN","DECN","INB",
    "?205?","NOPT","?207?","SC","RC",
    "SR","?211?","ANID","ORID","TSID",
    "?215?","LEAVE","?217?","EXAB","EXAM",
    "?220?","OUTB","?222?","OUTC","CAL00",
    "CAL01","CAL02","CAL03","CAL04","CAL05",
    "CAL06","CAL07","CAL08","CAL09","CAL10",
    "CAL11","CAL12","CAL13","CAL14","CAL15",
    "CAL16","CAL17","CAL18","CAL19","CAL20",
    "CAL21","CAL22","CAL23","CAL24","CAL25",
    "CAL26","CAL27","CAL28","CAL29","CAL30",
    "CAL31"
};

// Array numero argomenti (NBARGU) copiato esattamente dal Pascal originale
const unsigned char NBARGU[256] = {
    2,2,2,2,1,
    1,1,1,1,1,
    1,1,1,1,1,
    1,3,2,2,2,
    1,1,1,1,1,
    1,1,1,1,1,
    1,1,1,1,1,
    1,1,1,1,1,
    2,2,2,2,2,
    2,1,2,1,1,
    1,1,1,1,1,
    1,2,2,2,2,
    1,1,1,1,1,
    1,1,1,1,1,
    1,1,1,1,1,
    1,1,1,2,1,
    1,1,1,1,1,
    1,1,1,1,1,
    1,1,1,1,1,
    1,2,2,2,2,
    2,2,2,2,1,
    1,1,2,1,1,
    1,1,2,2,1,
    1,2,2,1,1,
    3,3,1,1,3,
    3,3,3,1,1,
    1,1,1,1,1,
    1,1,1,1,1,
    1,1,1,1,1,
    1,1,1,1,1,
    1,1,1,1,1,
    1,1,1,1,1,
    1,1,1,1,1,
    1,1,1,1,1,
    1,1,1,1,1,
    1,1,1,1,1,
    1,1,1,1,1,
    1,1,1,1,1,
    1,1,1,1,1,
    1,1,1,1,1,
    1,1,1,1,1,
    1,1,1,1,1,
    1,1,2,2,2,
    1,1,1,1,1,
    1,1,1,1,2,
    2,2,2,2,2,
    2,2,2,2,2,
    2,2,2,2,2,
    2,2,2,2,2,
    2,2,2,2,2,
    2,2,2,2,2,
    2
};

// Costanti per i salti relativi (dal Pascal originale)
const int JRPLUS[5] = {JRNZP, JRNCP, JRP, JRZP, JRCP};
const int JRMINUS[6] = {JRNZM, JRNCM, JRM, LOOP, JRZM, JRCM};
const int JR[11] = {JRNZP, JRNCP, JRP, JRZP, JRCP, JRNZM, JRNCM, JRM, LOOP, JRZM, JRCM};

#endif // PASM_CONSTANTS_H
