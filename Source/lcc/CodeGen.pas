//{$MODE DELPHI}
{--------------------------------------------------------------}
unit CodeGen;

{$IFDEF FPC}
  {$MODE Delphi}
{$ENDIF}

{--------------------------------------------------------------}
interface

uses Output, SysUtils;

// Core instruction emission
procedure EmitInst(const inst: string); overload;
procedure EmitInst(const inst, operand: string); overload;
procedure EmitInst(const inst, operand, comment: string); overload;
procedure EmitInstComment(const inst, comment: string); overload;
procedure EmitComment(const comment: string);
procedure EmitBlankLine;

// Label management
function NewLabel: string;
procedure PostLabel(L: string);

// Register load helpers
procedure load_x(s: string);
procedure load_y(s: string);

// High-level store functions (Task 4.1.1)
procedure StoreByteToReg(adr: integer; const name: string);
procedure StoreWordToReg(adr: integer; const name: string);

// High-level store functions for local variables (Task 4.1.2)
procedure StoreByteToLocal(adr: integer; const name: string);
procedure StoreWordToLocal(adr: integer; const name: string);

// High-level store functions for XRAM variables (Task 4.1.3)
procedure StoreByteToXram(adr: integer; const name: string);
procedure StoreWordToXram(adr: integer; const name: string);

// High-level load functions for global variables in registers (Task 4.1.4)
procedure LoadByteFromReg(adr: integer; const name: string);
procedure LoadWordFromReg(adr: integer; const name: string);

// High-level load functions for local variables (Task 4.1.5)
procedure LoadByteFromLocal(adr: integer; const name: string);
procedure LoadWordFromLocal(adr: integer; const name: string);

// High-level load functions for XRAM variables (Task 4.1.6)
procedure LoadByteFromXram(adr: integer; const name: string);
procedure LoadWordFromXram(adr: integer; const name: string);

// High-level array element access functions - byte (Task 4.1.7)
procedure LoadArrayByteFromReg(adr: integer; const name: string);
procedure LoadArrayByteFromXram(adr: integer; const name: string);
procedure StoreArrayByteToReg(adr: integer; const name: string);
procedure StoreArrayByteToXram(adr: integer; const name: string);

// High-level array element access functions - word (Task 4.1.8)
procedure LoadArrayWordFromReg(adr: integer; const name: string);
procedure LoadArrayWordFromXram(adr: integer; const name: string);
procedure StoreArrayWordToReg(adr: integer; const name: string);
procedure StoreArrayWordToXram(adr: integer; const name: string);

// Pointer dereferencing functions (Task 4.1.10)
procedure LoadPointerContentXram(const pnttyp, name: string);
procedure LoadPointerContentReg(const pnttyp, name: string);
procedure LoadAddressOf(adr: integer; const name: string; isXram: boolean);

procedure CompSmOrEq;
procedure CompGrOrEq;
procedure CompGreater;
procedure CompSmaller;
procedure CompEqual;
procedure CompNotEqual;
procedure BranchAbs(L: string);
procedure Branch(L: string);
procedure BranchAbsFalse(L: string);
procedure BranchFalse(L: string);
procedure Negate;
procedure Push;
procedure PopAdd;
procedure PopSub;
procedure PopMul;
procedure PopDiv;
procedure PopOr;
procedure PopSr;
procedure PopSl;
procedure PopXor;
procedure PopAnd;
procedure PopMod;
procedure NotIt;
procedure varxram(Value, adr, size: integer; nm: string);
procedure varxarr(Value: RawByteString; adr, size: integer; nm, typ: string);
procedure varcode(Value, adr, size: integer; nm: string);
procedure varfcode(Value: RawByteString; nm: string);
procedure varcarr(Value: RawByteString; adr, size: integer; nm, typ: string);
procedure varfarr(Value: RawByteString; size: integer; nm: string);
procedure varreg(Value, adr, size: integer; nm: string);
procedure varrarr(Value: RawByteString; adr, size: integer; nm, typ: string);

type OpTypes = (byte, word, floatp);


var
  LCount: integer;
  libins: array [0..100] of boolean;
  libname: array [0..100] of string;
  libcnt: integer;
  pc: string;
  isword: boolean;
  isfloat: boolean;
  pushcnt: integer;
  optype: OpTypes;


const   { Math }
  MUL8 = 0;
  DIVMOD8 = 1;
  SR8 = 3;
  SL8 = 4;
  XOR8 = 5;

  MUL16 = 12;
  DIV16 = 13;
  MOD16 = 14;
  AND16 = 15;
  OR16 = 16;
  XOR16 = 21;
  SR16 = 17;
  SL16 = 18;
  NOT16 = 19;
  NEG16 = 20;

  CPE16 = 30;
  CPNE16 = 31;
  CPS16 = 32;
  CPG16 = 33;
  CPSE16 = 34;
  CPGE16 = 35;

  FloatXReg = 16; // FloatXreg @ 0x10-0x17; used as Primary floating-point store
  FloatYReg = 24; // FloatYreg @ 0x18-0x1F

  {--------------------------------------------------------------}
implementation

{--------------------------------------------------------------}
{ Core Instruction Emission }

procedure EmitInst(const inst: string);
begin
  WritLn(#9 + inst);
end;

procedure EmitInst(const inst, operand: string);
begin
  WritLn(#9 + inst + #9 + operand);
end;

procedure EmitInst(const inst, operand, comment: string);
begin
  WritLn(#9 + inst + #9 + operand + #9 + '; ' + comment);
end;

procedure EmitInstComment(const inst, comment: string);
begin
  WritLn(#9 + inst + #9 + '; ' + comment);
end;

procedure EmitComment(const comment: string);
begin
  WritLn(#9 + '; ' + comment);
end;

procedure EmitBlankLine;
begin
  WritLn('');
end;

{--------------------------------------------------------------}
{ Generate a Unique Label }

function NewLabel: string;
var S: string;
begin
   Str(LCount, S);
   result := 'LB' + S;
   Inc(LCount);
end;

{--------------------------------------------------------------}
{ Post a Label To Output }

procedure PostLabel(L: string);
begin
   WritLn('  '+L+':');
end;

{--------------------------------------------------------------}
{ Load X register pair with address }

procedure load_x(s: string);
begin
  EmitInst('LP', '4', 'Load XL');
  EmitInst('LIA', 'LB(' + s + ')');
  EmitInst('EXAM');
  EmitInst('LP', '5', 'Load XH');
  EmitInst('LIA', 'HB(' + s + ')');
  EmitInst('EXAM');
end;

{--------------------------------------------------------------}
{ High-level Store functions (Task 4.1.1) }

procedure StoreByteToReg(adr: integer; const name: string);
begin
  if adr <= 63 then
    EmitInst('LP', IntToStr(adr), 'Store result in ' + name)
  else
    EmitInst('LIP', IntToStr(adr), 'Store result in ' + name);
  EmitInst('EXAM');
end;

procedure StoreWordToReg(adr: integer; const name: string);
begin
  if adr < 64 then
    EmitInst('LP', IntToStr(adr), 'Store 16bit variable ' + name)
  else
    EmitInst('LIP', IntToStr(adr), 'Store 16bit variable ' + name);
  EmitInstComment('EXAM', 'LB');
  EmitInst('EXAB');
  EmitInstComment('INCP', 'HB');
  EmitInst('EXAM');
end;

{--------------------------------------------------------------}
{ High-level Store functions for local variables (Task 4.1.2) }

procedure StoreByteToLocal(adr: integer; const name: string);
begin
  EmitInst('EXAB');  // temp save A (value to store) to B
  EmitInst('LDR');   // get stack ptr R to A
  EmitInst('ADIA', IntToStr(adr + 2 + pushcnt));  // add relative address
  EmitInst('STP');   // move result to P (absolute address)
  EmitInst('EXAB');  // restore A
  EmitInstComment('EXAM', 'Store result in ' + name);  // store value to P location
end;

procedure StoreWordToLocal(adr: integer; const name: string);
begin
  EmitInst('PUSH'); Inc(pushcnt);  // temp save A
  EmitInst('LDR');   // we overwrite A here
  EmitInst('ADIA', IntToStr(adr + 2 + pushcnt));  // adr + size + pushcnt
  EmitInst('STP');
  EmitInst('POP'); Dec(pushcnt);   // restore A
  EmitInstComment('EXAM', 'LB - Store result in ' + name);
  EmitInst('EXAB');
  EmitInst('DECP');
  EmitInstComment('EXAM', 'HB');
end;

{--------------------------------------------------------------}
{ High-level Store functions for XRAM variables (Task 4.1.3) }

procedure StoreByteToXram(adr: integer; const name: string);
begin
  if adr <> -1 then
    EmitInst('LIDP', IntToStr(adr), 'Store result in ' + name)
  else
    EmitInst('LIDP', name, 'Store result in ' + name);
  EmitInst('STD');
end;

procedure StoreWordToXram(adr: integer; const name: string);
begin
  if adr <> -1 then
    EmitInst('LIDP', IntToStr(adr), 'Store 16bit variable ' + name)
  else
    EmitInst('LIDP', name, 'Store 16bit variable ' + name);
  EmitInstComment('STD', 'LB');
  EmitInst('EXAB');
  if (adr <> -1) and ((adr + 1) div 256 = adr div 256) then
    EmitInst('LIDL', 'LB(' + IntToStr(adr) + '+1)')
  else if adr <> -1 then
    EmitInst('LIDP', IntToStr(adr) + '+1')
  else
    EmitInst('LIDP', name + '+1');  // PASM doesn't parse "name + 1"
  EmitInstComment('STD', 'HB');
end;

{--------------------------------------------------------------}
{ High-level Load functions for global variables in registers (Task 4.1.4) }

procedure LoadByteFromReg(adr: integer; const name: string);
begin
  if adr < 64 then
    EmitInst('LP', IntToStr(adr), 'Load variable ' + name)
  else
    EmitInst('LIP', IntToStr(adr), 'Load variable ' + name);
  EmitInst('LDM');
end;

procedure LoadWordFromReg(adr: integer; const name: string);
begin
  if adr < 64 then
    EmitInst('LP', IntToStr(adr + 1), 'Load 16bit variable ' + name)
  else
    EmitInst('LIP', IntToStr(adr + 1), 'Load 16bit variable ' + name);
  EmitInstComment('LDM', 'HB');
  EmitInst('EXAB');
  EmitInstComment('DECP', 'LB');
  EmitInst('LDM');
end;

{--------------------------------------------------------------}
{ High-level Load functions for local variables (Task 4.1.5) }

procedure LoadByteFromLocal(adr: integer; const name: string);
begin
  EmitInst('LDR');
  EmitInst('ADIA', IntToStr(adr + 2 + pushcnt));
  EmitInst('STP');
  EmitInstComment('LDM', 'Load variable ' + name);
end;

procedure LoadWordFromLocal(adr: integer; const name: string);
begin
  EmitInst('LDR');
  EmitInst('ADIA', IntToStr(adr + 1 + pushcnt));
  EmitInst('STP');
  EmitInstComment('LDM', 'HB - Load variable ' + name);
  EmitInst('EXAB');
  EmitInst('INCP');
  EmitInstComment('LDM', 'LB');
end;

{--------------------------------------------------------------}
{ High-level Load functions for XRAM variables (Task 4.1.6) }

procedure LoadByteFromXram(adr: integer; const name: string);
begin
  if adr <> -1 then
    EmitInst('LIDP', IntToStr(adr), 'Load variable ' + name)
  else
    EmitInst('LIDP', name, 'Load variable ' + name);
  EmitInst('LDD');
end;

procedure LoadWordFromXram(adr: integer; const name: string);
begin
  if adr <> -1 then
    EmitInst('LIDP', IntToStr(adr + 1), 'Load 16bit variable ' + name)
  else
    EmitInst('LIDP', name + '+1', 'Load 16bit variable ' + name);
  EmitInstComment('LDD', 'HB');
  EmitInst('EXAB');
  if (adr <> -1) and ((adr + 1) div 256 = adr div 256) then
    EmitInst('LIDL', 'LB(' + IntToStr(adr) + ')')
  else if adr <> -1 then
    EmitInst('LIDP', IntToStr(adr))
  else
    EmitInst('LIDP', name);
  EmitInstComment('LDD', 'LB');
end;

{--------------------------------------------------------------}
{ High-level array element access functions - byte (Task 4.1.7) }

procedure LoadArrayByteFromReg(adr: integer; const name: string);
begin
  EmitInst('LIB', IntToStr(adr), 'Load array element from ' + name);
  EmitInst('LP', '3');
  EmitInst('ADM');
  EmitInst('EXAB');
  EmitInst('STP');
  EmitInst('LDM');
end;

procedure LoadArrayByteFromXram(adr: integer; const name: string);
begin
  EmitInstComment('PUSH', 'Load array element from ' + name); Inc(pushcnt);
  EmitInst('LP', '5', 'HB of address');
  if adr <> -1 then
  begin
    EmitInst('LIA', 'HB(' + IntToStr(adr) + '-1)');
    EmitInst('EXAM');
    EmitInst('LP', '4', 'LB');
    EmitInst('LIA', 'LB(' + IntToStr(adr) + '-1)');
  end else
  begin
    EmitInst('LIA', 'HB(' + name + '-1)');
    EmitInst('EXAM');
    EmitInst('LP', '4', 'LB');
    EmitInst('LIA', 'LB(' + name + '-1)');
  end;
  EmitInst('EXAM');
  EmitInst('POP'); Dec(pushcnt);
  EmitInst('LIB', '0');
  EmitInst('ADB');
  EmitInst('POP'); Dec(pushcnt);
  EmitInst('EXAB');
  EmitInst('POP'); Dec(pushcnt);
  EmitInst('IYS');
  EmitInst('EXAB');
  EmitInst('IYS');
end;

procedure StoreArrayByteToReg(adr: integer; const name: string);
begin
  EmitInst('LIB', IntToStr(adr), 'Store array element from ' + name);
  EmitInst('LP', '3');
  EmitInst('ADM');
  EmitInst('EXAB');
  EmitInst('STP');
  EmitInst('POP'); Dec(pushcnt);
  EmitInst('EXAM');
end;

procedure StoreArrayByteToXram(adr: integer; const name: string);
begin
  EmitInstComment('PUSH', 'Store array element from ' + name); Inc(pushcnt);
  EmitInst('LP', '7', 'HB of address');
  if adr <> -1 then
  begin
    EmitInst('LIA', 'HB(' + IntToStr(adr) + '-1)');
    EmitInst('EXAM');
    EmitInst('LP', '6', 'LB');
    EmitInst('LIA', 'LB(' + IntToStr(adr) + '-1)');
  end else
  begin
    EmitInst('LIA', 'HB(' + name + '-1)');
    EmitInst('EXAM');
    EmitInst('LP', '6', 'LB');
    EmitInst('LIA', 'LB(' + name + '-1)');
  end;
  EmitInst('EXAM');
  EmitInst('POP'); Dec(pushcnt);
  EmitInst('LIB', '0');
  EmitInst('ADB');
  EmitInst('POP'); Dec(pushcnt);
  EmitInst('IYS');
end;

{--------------------------------------------------------------}
{ High-level array element access functions - word (Task 4.1.8) }

procedure LoadArrayWordFromReg(adr: integer; const name: string);
begin
  EmitInst('RC');
  EmitInst('SL');
  EmitInst('LII', IntToStr(adr), 'Load array element from ' + name);
  EmitInst('LP', '0');
  EmitInst('ADM');
  EmitInst('EXAM');
  EmitInst('STP');
  EmitInst('INCP');
  EmitInst('POP'); Dec(pushcnt);
  EmitInst('EXAM');
  EmitInst('DECP');
  EmitInst('POP'); Dec(pushcnt);
  EmitInst('EXAM');
end;

procedure LoadArrayWordFromXram(adr: integer; const name: string);
begin
  EmitInst('RC');
  EmitInst('SL');
  EmitInstComment('PUSH', 'Load array element from ' + name); Inc(pushcnt);
  EmitInst('LP', '7', 'HB of address');
  if adr <> -1 then
  begin
    EmitInst('LIA', 'HB(' + IntToStr(adr) + '-1)');
    EmitInst('EXAM');
    EmitInst('LP', '6', 'LB');
    EmitInst('LIA', 'LB(' + IntToStr(adr) + '-1)');
  end else
  begin
    EmitInst('LIA', 'HB(' + name + '-1)');
    EmitInst('EXAM');
    EmitInst('LP', '6', 'LB');
    EmitInst('LIA', 'LB(' + name + '-1)');
  end;
  EmitInst('EXAM');
  EmitInst('POP'); Dec(pushcnt);
  EmitInst('LIB', '0');
  EmitInst('ADB');
  EmitInst('POP'); Dec(pushcnt);
  EmitInst('EXAB');
  EmitInst('POP'); Dec(pushcnt);
  EmitInst('IYS');
  EmitInst('EXAB');
  EmitInst('IYS');
end;

procedure StoreArrayWordToReg(adr: integer; const name: string);
begin
  EmitInst('RC');
  EmitInst('SL');
  EmitInst('LII', IntToStr(adr), 'Store array element from ' + name);
  EmitInst('LP', '0');
  EmitInst('ADM');
  EmitInst('EXAM');
  EmitInst('STP');
  EmitInst('INCP');
  EmitInst('POP'); Dec(pushcnt);
  EmitInst('EXAM');
  EmitInst('DECP');
  EmitInst('POP'); Dec(pushcnt);
  EmitInst('EXAM');
end;

procedure StoreArrayWordToXram(adr: integer; const name: string);
begin
  EmitInst('RC');
  EmitInst('SL');
  EmitInstComment('PUSH', 'Store array element from ' + name); Inc(pushcnt);
  EmitInst('LP', '7', 'HB of address');
  if adr <> -1 then
  begin
    EmitInst('LIA', 'HB(' + IntToStr(adr) + '-1)');
    EmitInst('EXAM');
    EmitInst('LP', '6', 'LB');
    EmitInst('LIA', 'LB(' + IntToStr(adr) + '-1)');
  end else
  begin
    EmitInst('LIA', 'HB(' + name + '-1)');
    EmitInst('EXAM');
    EmitInst('LP', '6', 'LB');
    EmitInst('LIA', 'LB(' + name + '-1)');
  end;
  EmitInst('EXAM');
  EmitInst('POP'); Dec(pushcnt);
  EmitInst('LIB', '0');
  EmitInst('ADB');
  EmitInst('POP'); Dec(pushcnt);
  EmitInst('EXAB');
  EmitInst('POP'); Dec(pushcnt);
  EmitInst('IYS');
  EmitInst('EXAB');
  EmitInst('IYS');
end;

{--------------------------------------------------------------}
{ Pointer dereferencing functions (Task 4.1.10) }

procedure LoadPointerContentXram(const pnttyp, name: string);
begin
  EmitInst('LP', '4', 'XL');
  EmitInst('EXAM');
  EmitInst('LP', '5', 'XH');
  EmitInst('EXAB');
  EmitInst('EXAM');
  EmitInst('DX');
  if pnttyp <> 'word' then
  begin
    EmitInstComment('IXL', 'Load content *' + name);
  end else
  begin
    EmitInstComment('IXL', 'Load content LB *' + name);
    EmitInst('EXAB');
    EmitInstComment('IXL', 'Load content HB *' + name);
    EmitInst('EXAB');
  end;
end;

procedure LoadPointerContentReg(const pnttyp, name: string);
begin
  EmitInstComment('STP', 'Set P');
  if pnttyp <> 'word' then
  begin
    EmitInstComment('LDM', 'Load content *' + name);
  end else
  begin
    EmitInstComment('LDM', 'Load content LB *' + name);
    EmitInst('EXAB');
    EmitInst('INCP');
    EmitInstComment('LDM', 'Load content HB *' + name);
    EmitInst('EXAB');
  end;
end;

procedure LoadAddressOf(adr: integer; const name: string; isXram: boolean);
begin
  if isXram then
  begin
    if adr <> -1 then
    begin
      EmitInst('LIA', 'LB(' + IntToStr(adr) + ')', '&' + name);
      EmitInst('LIB', 'HB(' + IntToStr(adr) + ')', '&' + name);
    end else
    begin
      EmitInst('LIA', 'LB(' + name + ')', '&' + name);
      EmitInst('LIB', 'HB(' + name + ')', '&' + name);
    end;
  end else
  begin
    EmitInst('LIA', IntToStr(adr), '&' + name);
  end;
end;

{--------------------------------------------------------------}

procedure load_y(s: string);
begin
  EmitInst('LP', '6', 'Load YL');
  EmitInst('LIA', 'LB(' + s + ')');
  EmitInst('EXAM');
  EmitInst('LP', '7', 'Load YH');
  EmitInst('LIA', 'HB(' + s + ')');
  EmitInst('EXAM');
end;


{--------------------------------------------------------------}
{ Inserts library code for used libs }

procedure addlib(lib: integer);
begin
  if libcnt = 0 then
    AddLibText('; LIB Code'#13#10);
  Inc(libcnt);

  if (libins[lib] = False) then
  begin
    libins[lib] := True;
    AddLibText('.include ' + libname[lib] + '.lib'#13#10);
  end;
end;


{--------------------------------------------------------------}
{ Generates init code for a var in xram }

procedure varxram(Value, adr, size: integer; nm: string);
begin
  EmitInst('LIDP', IntToStr(adr), 'Variable ' + nm + ' = ' + IntToStr(Value));
  if size = 1 then
  begin
                {if value = 0 then
                        EmitInst('RA')
                else           }
    EmitInst('LIA', IntToStr(Value));
    EmitInst('STD');
  end
  else if size = 2 then
  begin
                {if value mod 256 = 0 then
                        EmitInst('RA')
                else     }
    EmitInst('LIA', 'LB(' + IntToStr(Value) + ')');

    EmitInst('STD');
    if adr div 256 = (adr + 1) div 256 then
      EmitInst('LIDL', 'LB(' + IntToStr(adr + 1) + ')')
    else
      EmitInst('LIDP', IntToStr(adr + 1));

    if Value mod 256 <> Value div 256 then
                {if value div 256 = 0 then
                        EmitInst('RA')
                else      }
      EmitInst('LIA', 'HB(' + IntToStr(Value) + ')');
    EmitInst('STD');
  end
  else if size = 8 then
    EmitComment('varxram - Unsupported float');

  EmitBlankLine;
end;
{--------------------------------------------------------------}


{--------------------------------------------------------------}
{ Generates init code for an array in xram }

procedure varxarr(Value: RawByteString; adr, size: integer; nm, typ: string);
var
  i: integer;
  v, c: integer;
  s: string;
begin

  if (size = 0) then exit;

  s := '';
  for i := 1 to size do
  begin
    if i <= length(Value) then
    begin
      if ((typ = 'byte') or (typ = 'char')) then
        s := s + IntToStr(Ord(Value[i]))
      else if typ = 'word' then
        s := s + IntToStr(256 * Ord(Value[i * 2 - 1]) + Ord(Value[i * 2]))
      else if typ = 'float' then s := ''; // TODO  !!
    end
    else
      s := s + '0';

    if i < size then
      s := s + ', ';
  end;

  if ((typ = 'byte') or (typ = 'char')) and (size <= 5) then
  begin
    // Set up address and write 1st byte
    EmitInst('LIDP', IntToStr(adr), 'Variable ' + nm + ' = (' + s + ')');
    v := Ord(Value[1]);
        { if v = 0 then
                EmitInst('RA')
        else  }
    EmitInst('LIA', IntToStr(v));
    EmitInst('STD');

    c := v;
    if size > 1 then for i := 2 to size do
      begin
        if (adr + i - 2) div 256 = (adr + i - 1) div 256 then
          EmitInst('LIDL', 'LB(' + IntToStr(adr + i - 1) + ')')
        else
          EmitInst('LIDP', IntToStr(adr + i - 1));

        if i <= length(Value) then
          v := Ord(Value[i])
        else
          v := 0;

        if v <> c then
        begin
                        {if v = 0 then
                                EmitInst('RA')
                        else         }
          EmitInst('LIA', IntToStr(v));
          EmitInst('STD');
        end
        else
          EmitInst('STD');
        c := v;
      end;
  end
  else if ((typ = 'byte') or (typ = 'char')) then
  begin
    EmitComment('Variable ' + nm + ' = (' + s + ')');
    load_x(nm + '-1');
    load_y(nm + '-1');
    EmitInst('LII', IntToStr(size), 'Load I as counter');
    EmitInst('IXL');
    EmitInst('IYS');
    EmitInst('DECI');
    EmitInst('JRNZM', '4');

    addasm(nm + ':'#9'; Variable init data ' + nm);
    s := #9'.DB'#9 + s;
    addasm(s);
    addasm('');
  end
  else if (typ = 'word') then
  begin
    EmitComment('Variable ' + nm + ' = (' + s + ')');
    load_x(nm + '-1');
    load_y(nm + '-1');
    EmitInst('LII', IntToStr(size * 2), 'Load I as counter');
    EmitInst('IXL');
    EmitInst('IYS');
    EmitInst('DECI');
    EmitInst('JRNZM', '4');

    addasm(nm + ':'#9'; Variable init data ' + nm);
    s := #9'.DW'#9 + s;
    addasm(s);
    addasm('');
  end
  else if (typ = 'float') then
  begin
    //...  TODO - float
    EmitComment('varxarr - Unsupported float ' + nm);
  end;

  EmitBlankLine;
end;
{--------------------------------------------------------------}


{--------------------------------------------------------------}
{ Generates init code for a var in code space }

procedure varcode(Value, adr, size: integer; nm: string);
begin
  if Value = -1 then Value := 0;
  addasm(nm + ':'#9'; Variable ' + nm + ' = ' + IntToStr(Value));
  if size = 1 then
    addasm(#9'.DB'#9 + IntToStr(Value))
  else if size = 2 then
    addasm(#9'.DW'#9 + IntToStr(Value));
  addasm('');
end;
{--------------------------------------------------------------}

{--------------------------------------------------------------}
{ Generates init code for an array in code space }

procedure varcarr(Value: RawByteString; adr, size: integer; nm, typ: string);
var
  i: integer;
  s: string;
begin

  if (size = 0) then exit;
  s := '';
  for i := 1 to size do
  begin
    if i <= length(Value) then
    begin
      if not ( typ = 'word') then s := s + IntToStr(Ord(Value[i]))
      else if typ = 'word' then s := s + IntToStr(256 * Ord(Value[i * 2 - 1]) + Ord(Value[i * 2]));
    end
    else
      s := s + '0';

    if i < size then
      s := s + ', ';
  end;

  addasm(nm + ':'#9'; Variable ' + nm + ' = (' + s + ')');
  if (typ = 'char') or (typ = 'char') or (typ = 'float') then
    s := #9'.DB'#9 + s
  else if (typ = 'word') then
    s := #9'.DW'#9 + s;
  addasm(s);
  addasm('');
end;

{ Generates init code for a float in code space }

procedure varfcode(Value: RawByteString; nm: string);
var
  i: integer;
  s: string;
begin
  s := '';
  for i := 1 to 7 do begin
    s := s + '0x'+IntToHex(ord(Value[i]), 2) + ',';
  end;
  s := s + '0x'+IntToHex(ord(Value[8]), 2);


  addasm(nm + ':'#9'; Floating-point  ' + nm ) ;
  s := #9'.DB'#9 + s;
  addasm(s);
  addasm('');
end;
{--------------------------------------------------------------}

{ Generates init code for a float array in code space }
procedure varfarr(Value: RawByteString; size: integer; nm: string);
var
  i, j : integer;
  s: string;
begin

  addasm(nm + ':'#9'; Floating-point array ' + nm
            + ' array (' + IntToStr( size ) + ')' ) ;
  for j := 0 to size-1 do begin
    s := '';
    for i := 1 to 7 do s := s + '0x'+IntToHex(ord(Value[j*8 + i]), 2) + ',';
    s := s + '0x'+IntToHex(ord(Value[j*8 + 8]), 2);
    s := #9'.DB'#9 + s + ' ; ' + IntToStr( j ) ;
    addasm(s);
  end;
  addasm('');
end;
{--------------------------------------------------------------}

{--------------------------------------------------------------}
{ Generates init code for a variable in a register }

procedure varreg(Value, adr, size: integer; nm: string);
begin
  { Check for named register }
  if ((adr = 0) or (adr = 1)) and (size = 1) then
  begin
    if adr = 0 then
      EmitInst('LII', IntToStr(Value), 'I is ' + nm + ' = ' + IntToStr(Value))
    else
      EmitInst('LIJ', IntToStr(Value), 'J is ' + nm + ' = ' + IntToStr(Value));
    EmitBlankLine;
    exit;
  end;

  if adr <= 63 then
    EmitInst('LP', IntToStr(adr), 'Variable ' + nm + ' = ' + IntToStr(Value))
  else
  begin
    EmitInst('LIP', IntToStr(adr), 'Variable ' + nm + ' = ' + IntToStr(Value));
  end;
  if size = 1 then
  begin
    EmitInst('LIA', IntToStr(Value));
    EmitInst('EXAM');
  end
  else if size = 2 then
  begin
                { if value mod 256 = 0 then
                        EmitInst('RA')
                else    }
    EmitInst('LIA', 'LB(' + IntToStr(Value) + ')', 'LB');
    EmitInst('EXAM');

    EmitInst('INCP');
    if Value mod 256 <> Value div 256 then
                { if value div 256 = 0 then
                        EmitInst('RA')    }
    else
      EmitInst('LIA', 'HB(' + IntToStr(Value) + ')', 'HB');
    EmitInst('EXAM');
  end
  else if size = 8 then
  begin
    EmitComment('varreg - Unsupported float');
  end;
  EmitBlankLine;
end;
{--------------------------------------------------------------}


{--------------------------------------------------------------}
{ Generates init code for an array variable in a register }

procedure varrarr(Value: RawByteString; adr, size: integer; nm, typ: string);
var
  i: integer;
  s: string;
begin
  if size = 0 then exit;
  if typ = 'float' then EmitComment('varrarr - Unsupported float');

  s := '';
  for i := 1 to size do
  begin
    if i <= length(Value) then
    begin
      if typ <> 'word' then s := s + IntToStr(Ord(Value[i]))
      else if typ = 'word' then
        s := s + IntToStr(256 * Ord(Value[i * 2 - 1]) + Ord(Value[i * 2]));
    end
    else
      s := s + '0';

    if i < size then
      s := s + ', ';
  end;

  if typ <> 'word' then
    EmitInst('LII', IntToStr(size - 1), 'Variable ' + nm + ' = (' + s + ')')
  else
    EmitInst('LII', IntToStr(size * 2 - 1), 'Variable ' + nm + ' = (' + s + ')');
  EmitInst('LIDP', nm);
  if adr <= 63 then
    EmitInst('LP', IntToStr(adr))
  else
    EmitInst('LIP', IntToStr(adr));
  EmitInst('MVWD');

  addasm(nm + ':'#9'; Variable init data ' + nm);
  if typ <> 'word' then
    s := #9'.DB'#9 + s
  else
    s := #9'.DW'#9 + s;
  addasm(s);
  addasm('');

  EmitBlankLine;
end;
{--------------------------------------------------------------}


{---------------------------------------------------------------}
{ Comparison Equal }

procedure CompEqual;
begin
  if optype = word then
  begin
    EmitInst('LP', '0');
    EmitInst('EXAM');
    EmitInst('EXAB');
    EmitInst('LP', '1');
    EmitInst('EXAM');
    EmitInst('POP');
    Dec(pushcnt);
    EmitInst('EXAB');
    EmitInst('POP');
    Dec(pushcnt);
    EmitInst('CALL', 'LIB_CPE16', 'Compare for equal');
    addlib(CPE16);
  end
  else
  begin
    EmitInst('EXAB');
    EmitInst('POP');
    Dec(pushcnt);
    EmitInst('LP', '3');
    EmitInstComment('CPMA', 'Compare for equal');
    {EmitInst('RA')}
    EmitInst('LIA', '0');
    EmitInst('JRNZP', '2');
    EmitInst('DECA');
  end;
  EmitBlankLine;
end;


{---------------------------------------------------------------}
{ Comparison Not Equal }

procedure CompNotEqual;
begin
  if optype = word then
  begin
    EmitInst('LP', '0');
    EmitInst('EXAM');
    EmitInst('EXAB');
    EmitInst('LP', '1');
    EmitInst('EXAM');
    EmitInst('POP');
    Dec(pushcnt);
    EmitInst('EXAB');
    EmitInst('POP');
    Dec(pushcnt);
    EmitInst('CALL', 'LIB_CPNE16', 'Compare not equal');
    addlib(CPNE16);
  end
  else
  begin
    EmitInst('EXAB');
    EmitInst('POP');
    Dec(pushcnt);
    EmitInst('LP', '3');
    EmitInstComment('CPMA', 'Compare for not equal');
    {EmitInst('RA')}
    EmitInst('LIA', '0');
    EmitInst('JRZP', '2');
    EmitInst('DECA');
  end;
  EmitBlankLine;
end;


{---------------------------------------------------------------}
{ Comparison Greater or Equal }

procedure CompGrOrEq;
begin
  if optype = word then
  begin
    EmitInst('LP', '0');
    EmitInst('EXAM');
    EmitInst('EXAB');
    EmitInst('LP', '1');
    EmitInst('EXAM');
    EmitInst('POP');
    Dec(pushcnt);
    EmitInst('EXAB');
    EmitInst('POP');
    Dec(pushcnt);
    EmitInst('CALL', 'LIB_CPGE16', 'Compare for Greater or Equal');
    addlib(CPGE16);
  end
  else
  begin
    EmitInst('EXAB');
    EmitInst('POP');
    Dec(pushcnt);
    EmitInst('EXAB');
    EmitInst('LP', '3');
    EmitInstComment('CPMA', 'Compare for Greater or Equal');
    {EmitInst('RA')}
    EmitInst('LIA', '0');
    EmitInst('JRCP', '2');
    EmitInst('DECA');
  end;
  EmitBlankLine;
end;


{---------------------------------------------------------------}
{ Comparison smaller or equal }

procedure CompSmOrEq;
begin
  if optype = word then
  begin
    EmitInst('LP', '0');
    EmitInst('EXAM');
    EmitInst('EXAB');
    EmitInst('LP', '1');
    EmitInst('EXAM');
    EmitInst('POP');
    Dec(pushcnt);
    EmitInst('EXAB');
    EmitInst('POP');
    Dec(pushcnt);
    EmitInst('CALL', 'LIB_CPSE16', 'Compare for smaller or equal');
    addlib(CPSE16);
  end
  else
  begin
    EmitInst('EXAB');
    EmitInst('POP');
    Dec(pushcnt);
    EmitInst('LP', '3');
    EmitInstComment('CPMA', 'Compare for smaller or equal');
    {EmitInst('RA')}
    EmitInst('LIA', '0');
    EmitInst('JRCP', '2');
    EmitInst('DECA');
  end;
  EmitBlankLine;
end;


{---------------------------------------------------------------}
{ Comparison Greater }

procedure CompGreater;
begin
  if optype = word then
  begin
    EmitInst('LP', '0');
    EmitInst('EXAM');
    EmitInst('EXAB');
    EmitInst('LP', '1');
    EmitInst('EXAM');
    EmitInst('POP');
    Dec(pushcnt);
    EmitInst('EXAB');
    EmitInst('POP');
    Dec(pushcnt);
    EmitInst('CALL', 'LIB_CPG16', 'Compare for greater');
    addlib(CPG16);
  end
  else
  begin
    EmitInst('EXAB');
    EmitInst('POP');
    Dec(pushcnt);
    EmitInst('LP', '3');
    EmitInstComment('CPMA', 'Compare for greater');
    {EmitInst('RA')}EmitInst('LIA', '0');
    EmitInst('JRNCP', '2');
    EmitInst('DECA');
  end;
  EmitBlankLine;
end;


{---------------------------------------------------------------}
{ Comparison smaller }

procedure CompSmaller;
begin
  if optype = word then
  begin
    EmitInst('LP', '0');
    EmitInst('EXAM');
    EmitInst('EXAB');
    EmitInst('LP', '1');
    EmitInst('EXAM');
    EmitInst('POP');
    Dec(pushcnt);
    EmitInst('EXAB');
    EmitInst('POP');
    Dec(pushcnt);
    EmitInst('CALL', 'LIB_CPS16', 'Compare for smaller');
    addlib(CPS16);
  end
  else
  begin
    EmitInst('EXAB');
    EmitInst('POP');
    Dec(pushcnt);
    EmitInst('EXAB');
    EmitInst('LP', '3');
    EmitInstComment('CPMA', 'Compare for smaller');
    {EmitInst('RA')}
    EmitInst('LIA', '0');
    EmitInst('JRNCP', '2');
    EmitInst('DECA');
  end;
  EmitBlankLine;
end;

{---------------------------------------------------------------}
{ Absolute Branch Unconditional  }

procedure BranchAbs(L: string);
begin
  EmitInst('JP', L);
end;

{---------------------------------------------------------------}
{ Branch Unconditional  }

procedure Branch(L: string);
begin
  EmitInst('RJMP', L);
end;


{---------------------------------------------------------------}
{ Branch False }

procedure BranchFalse(L: string);
begin
  EmitInst('TSIA', '255', 'Branch if false');
  EmitInst('JRZP', L);
  EmitBlankLine;
end;

{---------------------------------------------------------------}
{ Absolute Branch False }

procedure BranchAbsFalse(L: string);
begin
  EmitInst('TSIA', '255', 'Branch if false');
  EmitInst('JPZ', L);
  EmitBlankLine;
end;

{--------------------------------------------------------------}
{ Bitwise Not Primary }

procedure NotIt;
begin
  if optype = word then
  begin
    EmitInst('LP', '1');
    EmitInst('ORIM', '255');
    EmitInst('LP', '0');
    EmitInst('ORIM', '255');
    EmitInstComment('SBB', 'Negate');
    EmitInst('LP', '1');
    EmitInst('LDM');
    EmitInst('EXAB');
    EmitInst('LP', '0');
    EmitInst('LDM');
  end
  else
  begin
    EmitInst('LIB', '0');
    EmitInst('LP', '3');
    EmitInstComment('SBM', 'Negate');
    EmitInst('EXAB');
  end;
{    if optype = word then
    begin
        EmitInst('LP', '0');
        EmitInst('EXAM');
        EmitInst('LP', '1');
        EmitInst('EXAB');
        EmitInst('EXAM');
        EmitInst('LIA', '255');
        EmitInstComment('SBM', 'NOT HB');
        EmitInst('EXAB');
        EmitInst('EXAM');
        EmitInst('EXAB');
        EmitInst('LP', '0');
        EmitInstComment('SBM', 'NOT LB');
        EmitInst('EXAM');
    end else
    begin
        EmitInst('EXAB');
        EmitInst('LIA', '255');
        EmitInst('LP', '3');
        EmitInstComment('SBM', 'NOT');
        EmitInst('EXAB');
    end;}
end;

{--------------------------------------------------------------}


{--------------------------------------------------------------}
{ Negate Primary }

procedure Negate;
begin
  if optype = word then
  begin
    EmitInst('LP', '1');
    EmitInst('ORIM', '255');
    EmitInst('LP', '0');
    EmitInst('ORIM', '255');
    EmitInstComment('SBB', 'Negate');
    EmitInst('LP', '1');
    EmitInst('LDM');
    EmitInst('EXAB');
    EmitInst('LP', '0');
    EmitInst('LDM');
  end else if optype = floatp then
  begin
    EmitComment('TO-DO? Negate a floating point');
  end else
  begin
    EmitInst('LIB', '0');
    EmitInst('LP', '3');
    EmitInstComment('SBM', 'Negate');
    EmitInst('EXAB');
  end;
end;
{--------------------------------------------------------------}


{--------------------------------------------------------------}
{ Or TOS with Primary }

procedure PopOr;
begin
  if optype = word then
  begin
    EmitInst('LP', '0');
    EmitInst('EXAM');
    EmitInst('EXAB');
    EmitInst('LP', '1');
    EmitInst('EXAM');
    EmitInst('POP');
    Dec(pushcnt);
    EmitInstComment('ORMA', 'OR HB');
    EmitInst('LP', '0');
    EmitInst('POP');
    Dec(pushcnt);
    EmitInstComment('ORMA', 'OR LB');
    EmitInst('EXAM');
    EmitInst('EXAB');
    EmitInst('LP', '1');
    EmitInst('EXAM');
    EmitInst('EXAB');
  end else if optype = floatp then
  begin
    EmitComment('!!! USUPPORTED Floating-point OR');
  end else
  begin
    EmitInst('EXAB');
    EmitInst('POP');
    Dec(pushcnt);
    EmitInst('LP', '3');
    EmitInstComment('ORMA', 'OR');
    EmitInst('EXAB');
  end;
end;

{--------------------------------------------------------------}
{ Exclusive-Or TOS with Primary }

procedure PopXor;
begin
  if optype = word then
  begin
    EmitInst('LP', '0');
    EmitInst('EXAM');
    EmitInst('EXAB');
    EmitInst('LP', '1');
    EmitInst('EXAM');
    EmitInst('POP');
    Dec(pushcnt);
    EmitInst('EXAB');
    EmitInst('POP');
    Dec(pushcnt);
    EmitInst('CALL', 'LIB_XOR16', 'XOR');
    addlib(XOR16);
    addlib(XOR8);
  end else if optype = floatp then
  begin
    EmitComment('!!! UNSUPPORTED Floating-point XOR');
  end else
  begin
    EmitInst('EXAB');
    EmitInst('POP');
    Dec(pushcnt);
    EmitInst('CALL', 'LIB_XOR8', 'XOR');
    addlib(XOR8);
  end;
end;
{--------------------------------------------------------------}


{--------------------------------------------------------------}
{ Shift left TOS with Primary }

procedure PopSL;
begin
  if optype = word then
  begin
    EmitInst('LP', '0');
    EmitInst('EXAM');
    EmitInst('EXAB');
    EmitInst('LP', '1');
    EmitInst('EXAM');
    EmitInst('POP');
    Dec(pushcnt);
    EmitInst('EXAB');
    EmitInst('POP');
    Dec(pushcnt);
    EmitInst('CALL', 'LIB_SL16', 'Shift left');
    addlib(SL16);
  end else if optype = floatp then
  begin
    EmitComment('!!! UNSUPPORTED Floating-point Shift');
  end
  else
  begin
    EmitInst('EXAB');
    EmitInst('POP');
    Dec(pushcnt);
    EmitInst('CALL', 'LIB_SL8', 'Shift left');
    addlib(SL8);
  end;
end;
{--------------------------------------------------------------}


{--------------------------------------------------------------}
{ Shift right TOS with Primary }

procedure PopSR;
begin
  if optype = word then
  begin
    EmitInst('LP', '0');
    EmitInst('EXAM');
    EmitInst('EXAB');
    EmitInst('LP', '1');
    EmitInst('EXAM');
    EmitInst('POP');
    Dec(pushcnt);
    EmitInst('EXAB');
    EmitInst('POP');
    Dec(pushcnt);
    EmitInst('CALL', 'LIB_SR16', 'Shift right');
    addlib(SR16);
  end else if optype = floatp then
  begin
    EmitComment('!!! UNSUPPORTED Floating-point Shift');
  end else begin
    EmitInst('EXAB');
    EmitInst('POP');
    Dec(pushcnt);
    EmitInst('CALL', 'LIB_SR8', 'Shift right');
    addlib(SR8);
  end;
end;
{--------------------------------------------------------------}


{--------------------------------------------------------------}
{ Modulo TOS with Primary }

procedure PopMod;
begin
  if optype = word then
  begin
    EmitInst('LP', '0');
    EmitInst('EXAM');
    EmitInst('EXAB');
    EmitInst('LP', '1');
    EmitInst('EXAM');
    EmitInst('POP');
    Dec(pushcnt);
    EmitInst('EXAB');
    EmitInst('POP');
    Dec(pushcnt);
    EmitInst('CALL', 'LIB_MOD16', 'Modulo');
    addlib(MOD16);
  end
  else if optype = floatp then
  begin
    EmitComment('TO DO - Float Modulo');
  end else
  begin
    EmitInst('EXAB');
    EmitInst('POP');
    Dec(pushcnt);
    EmitInst('CALL', 'LIB_DIV8', 'Modulo');
    EmitInst('EXAB');
    addlib(DIVMOD8);
  end;
end;
{--------------------------------------------------------------}


{--------------------------------------------------------------}
{ And Primary with TOS }

procedure PopAnd;
begin
  if optype = word then
  begin
    EmitInst('LP', '0');
    EmitInst('EXAM');
    EmitInst('EXAB');
    EmitInst('LP', '1');
    EmitInst('EXAM');
    EmitInst('POP');
    Dec(pushcnt);
    EmitInstComment('ANMA', 'AND HB');
    EmitInst('LP', '0');
    EmitInst('POP');
    Dec(pushcnt);
    EmitInstComment('ANMA', 'AND LB');
    EmitInst('EXAM');
    EmitInst('EXAB');
    EmitInst('LP', '1');
    EmitInst('EXAM');
    EmitInst('EXAB');
  end else if optype = floatp then
  begin
    EmitComment('!!! UNSUPPORTED Floating-point AND');
  end else
  begin
    EmitInst('EXAB');
    EmitInst('POP');
    Dec(pushcnt);
    EmitInst('LP', '3');
    EmitInstComment('ANMA', 'AND');
    EmitInst('EXAB');
  end;
end;

{--------------------------------------------------------------}
// to review and verify !
procedure PopFloat;
var i: integer;
begin
    EmitInst('LP', '0x1F', 'Pop 8 bytes, to Yreg');
    // Need to make a smarter ASM block! like Push
    for i := 0 to 7 do begin
        EmitInst('POP');
        dec(pushcnt);
        EmitInst('EXAM');
        EmitInst('DECP');  // should we go up?
    end;
end;

{--------------------------------------------------------------}
{ Push Primary to Stack }

procedure Push;
var lb: String;
begin
  if optype = word then
  begin
    EmitInst('PUSH', '', 'word (A, then B)');
    Inc(pushcnt);
    EmitInst('EXAB');
    EmitInst('PUSH');
    Inc(pushcnt);
  end
  else if optype = floatp then
  begin
    EmitInst('LP', '0x'+IntToHex(FloatXReg,2), 'float');
    EmitInst('LIJ', '0x08');
    lb := NewLabel;
    PostLabel(lb);
    EmitInst('LDM');
    EmitInst('PUSH'); inc(pushcnt);
    EmitInst('INCP');
    EmitInst('DECJ');
    EmitInst('JRNZM', lb);
{
    for i := 0 to 7 do begin
      EmitInst('LDM');
      EmitInst('PUSH'); inc(pushcnt);
      EmitInst('INCP');
    end;
}
  end
  else  // byte or char
  begin
    EmitInst('PUSH', '', 'byte');
    Inc(pushcnt);
  end;
end;


{--------------------------------------------------------------}
{ Add TOS to Primary }

procedure PopAdd;
begin
  if optype = word then
  begin
    EmitInst('LP', '0');
    EmitInst('EXAM');
    EmitInst('EXAB');
    EmitInst('LP', '1');
    EmitInst('EXAM');
    EmitInst('POP');
    Dec(pushcnt);
    EmitInst('EXAB');
    EmitInst('POP'); dec(pushcnt);
    //      EmitInst('EXAB');
    EmitInst('LP', '0');
    EmitInstComment('ADB', 'Addition');
    EmitInst('LP', '1');
    EmitInst('LDM');
    EmitInst('EXAB');
    EmitInst('LP', '0');
    EmitInst('LDM');
  end
  else if optype = floatp then
  begin
    EmitComment('PopAdd float');
    PopFloat;
    EmitInst('LP', '0x10', 'FloatXReg --> Xreg; (Q) -> (P), I+1 times');
    EmitInst('LIQ', '0x'+IntToHex(FloatXReg,2));
    EmitInst('LII', '7');
    EmitInst('MVW');
    // Floating point addition
    EmitInst('CALL', '0x10C2', 'Xreg + Yreg -> Xreg ( PC-1403 only? )');
    EmitInst('LP', '0x'+IntToHex(FloatXReg,2), 'Xreg --> FloatXReg; (Q) -> (P), I+1 times');
    EmitInst('LIQ', '0x10');
    EmitInst('LII', '7');
    EmitInst('MVW');
  end else
  begin
    EmitInst('EXAB');
    EmitInst('POP');
    Dec(pushcnt);
    EmitInst('LP', '3');
    EmitInstComment('ADM', 'Addition');
    EmitInst('EXAB');
  end;
end;


{--------------------------------------------------------------}
{ Subtract TOS from Primary }

procedure PopSub;
begin
  if optype = word then
  begin
    EmitInst('LP', '1');
    EmitInst('EXAM');
    EmitInst('POP');
    Dec(pushcnt);
    EmitInst('EXAM');
    EmitInst('LP', '0');
    EmitInst('EXAM');
    EmitInst('POP');
    Dec(pushcnt);
    EmitInst('EXAM');
    EmitInstComment('SBB', 'Subtraction');
    EmitInst('LP', '1');
    EmitInst('LDM');
    EmitInst('EXAB');
    EmitInst('LP', '0');
    EmitInst('LDM');
  end else if optype = floatp then
  begin
    EmitComment('PopSub float');
    PopFloat;
    EmitInst('LP', '0x10', 'FloatXReg --> Xreg; (Q) -> (P), I+1 times');
    EmitInst('LIQ', '0x'+IntToHex(FloatXReg,2));
    EmitInst('LII', '7');
    EmitInst('MVW');
    // Floating point subtraction
    EmitInst('CALL', '0x10D8', 'Yreg - Xreg -> Xreg ( PC-1403 only? )');
    EmitInst('LP', '0x'+IntToHex(FloatXReg,2), 'Xreg --> FloatXReg; (Q) -> (P), I+1 times');
    EmitInst('LIQ', '0x10');
    EmitInst('LII', '7');
    EmitInst('MVW');
  end else
  begin
    EmitInst('EXAB');
    EmitInst('POP');
    Dec(pushcnt);
    EmitInst('LP', '3');
    EmitInst('EXAB');
    EmitInstComment('SBM', 'Subtraction');
    EmitInst('EXAB');
  end;
end;
{--------------------------------------------------------------}


{--------------------------------------------------------------}
{ Multiply TOS by Primary }

procedure PopMul;
begin
  if optype = word then
  begin
    EmitInst('LP', '0');
    EmitInst('EXAM');
    EmitInst('EXAB');
    EmitInst('LP', '1');
    EmitInst('EXAM');
    EmitInst('POP');
    Dec(pushcnt);
    EmitInst('EXAB');
    EmitInst('POP');
    Dec(pushcnt);
    EmitInst('CALL', 'LIB_MUL16', 'Multiplication');
    addlib(MUL16);
  end else if optype = floatp then
  begin
    EmitComment('PopMul float');
    PopFloat;
    EmitInst('LP', '0x10', 'FloatXReg --> Xreg; (Q) -> (P), I+1 times');
    EmitInst('LIQ', '0x'+IntToHex(FloatXReg,2));
    EmitInst('LII', '7');
    EmitInst('MVW');
    // Floating point multiplication
    EmitInst('CALL', '0x10E1', 'Yreg * Xreg -> Xreg ( PC-1403 only? )');
    EmitInst('LP', '0x'+IntToHex(FloatXReg,2), 'Xreg --> FloatXReg; (Q) -> (P), I+1 times');
    EmitInst('LIQ', '0x10');
    EmitInst('LII', '7');
    EmitInst('MVW');
  end else
  begin
    EmitInst('EXAB');
    EmitInst('POP');
    Dec(pushcnt);
    EmitInst('CALL', 'LIB_MUL8', 'Multiplication');
    addlib(MUL8);
  end;
end;

{--------------------------------------------------------------}
{ Divide Primary by TOS }

procedure PopDiv;
begin
  // TODO - HANDLE RUNTIME ERRORS! (EG. DIV BY ZERO)
  if optype = word then
  begin
    EmitComment ( '16 bit division B:A / J:I -> B:A' );
    EmitInst('LP', '0');
    EmitInst('EXAM');  // A -> I
    EmitInst('EXAB');
    EmitInst('LP', '1');
    EmitInst('EXAM');  // B -> J
    EmitInst('POP');
    Dec(pushcnt);
    EmitInst('EXAB');
    EmitInst('POP');
    Dec(pushcnt);
    EmitInst('CALL', 'LIB_DIV16', 'Division');
    addlib(DIV16);
  end else if optype = floatp then
  begin
    EmitComment('PopDiv float');
    PopFloat;
    EmitInst('LP', '0x10', 'FloatXReg --> Xreg; (Q) -> (P), I+1 times');
    EmitInst('LIQ', '0x'+IntToHex(FloatXReg,2));
    EmitInst('LII', '7');
    EmitInst('MVW');
    // Floating point division
    EmitInst('CALL', '0x10EA', 'Yreg / Xreg -> Xreg ( PC-1403 only? )');
    EmitInst('LP', '0x'+IntToHex(FloatXReg,2), 'Xreg --> FloatXReg; (Q) -> (P), I+1 times');
    EmitInst('LIQ', '0x10');
    EmitInst('LII', '7');
    EmitInst('MVW');
  end else
  begin
    EmitInst('EXAB');
    EmitInst('POP');
    Dec(pushcnt);
    EmitInst('CALL', 'LIB_DIV8', 'Division');
    addlib(DIVMOD8);
  end;
end;
{--------------------------------------------------------------}


begin
  for libcnt := 0 to 100 do
    libins[libcnt] := False;
  asmcnt := 0;
  libcnt := 0;

  libname[MUL8] := 'mul8';
  libname[DIVMOD8] := 'divmod8';
  libname[SR8] := 'sr8';
  libname[SL8] := 'sl8';
  libname[XOR8] := 'xor8';

  libname[MUL16] := 'mul16';
  libname[DIV16] := 'div16';
  libname[MOD16] := 'mod16';
  libname[AND16] := 'and16';
  libname[OR16] := 'or16';
  libname[XOR16] := 'xor16';
  libname[SR16] := 'sr16';
  libname[SL16] := 'sl16';
  libname[NOT16] := 'not16';
  libname[NEG16] := 'neg16';

  libname[CPE16] := 'cpe16';
  libname[CPNE16] := 'cpne16';
  libname[CPS16] := 'cps16';
  libname[CPG16] := 'cpg16';
  libname[CPSE16] := 'cpse16';
  libname[CPGE16] := 'cpge16';

end.

