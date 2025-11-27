unit SymbolTable;

{$IFDEF FPC}
  {$MODE Delphi}
{$ENDIF}

interface

uses SysUtils, Errors;

type
  TVarInfo = record
    VarName: string;
    Xram: Boolean;
    At: Boolean;
    Arr: Boolean;
    Local: Boolean;
    Pointer: Boolean;
    Address: Integer;
    LocProc: Integer;
    Size: Integer;
    InitN: Integer;
    InitF: Double;
    Typ: string;
    PntTyp: string;
    Inits: string;
  end;

  TProcInfo = record
    ProcName: string;
    ProcCode: string;
    HasReturn: Boolean;
    ReturnIsWord: Boolean;
    ReturnType: string;
    Params: string;
    ParCnt: Integer;
    ParName: array[0..20] of string;
    ParTyp: array[0..20] of string;
    LocCnt: Integer;
    LocName: array[0..20] of string;
    LocTyp: array[0..20] of string;
    IsCalled: Boolean;
  end;

const
  MAX_VARS = 1000;
  MAX_PROCS = 1000;

// Var API
function GetVarCount: Integer;
function GetVarInfo(i: Integer): TVarInfo;
function FindVar(const name: string; out idx: Integer): Boolean;
function AddVar(const info: TVarInfo): Integer; // returns index
function AllocVar(xr, at: Boolean; size, adr: Integer): Integer;
function IsVarAtAdr(adr, size: Integer): Boolean; overload;
function IsVarAtAdr(adr, size: Integer; out foundIdx: Integer): Boolean; overload;
procedure RemoveLocalVars(procIndex: Integer);

// Proc API
function GetProcCount: Integer;
function GetProcInfo(i: Integer): TProcInfo;
function FindProc(const name: string; out idx: Integer): Boolean;
function AddProc(const info: TProcInfo): Integer;

implementation

var
  VarList: array[0..MAX_VARS-1] of TVarInfo;
  VarCount: Integer = 0;
  ProcList: array[0..MAX_PROCS-1] of TProcInfo;
  ProcCount: Integer = 0;

function GetVarCount: Integer;
begin
  Result := VarCount;
end;

function GetVarInfo(i: Integer): TVarInfo;
begin
  if (i < 0) or (i >= VarCount) then
    raise Exception.Create('GetVarInfo: index out of range');
  Result := VarList[i];
end;

function FindVar(const name: string; out idx: Integer): Boolean;
var i: Integer;
begin
  Result := False; idx := -1;
  for i := 0 to VarCount - 1 do
  begin
    if SameText(VarList[i].VarName, name) then
    begin
      idx := i; Result := True; Exit;
    end;
  end;
end;

function AddVar(const info: TVarInfo): Integer;
begin
  if VarCount >= MAX_VARS then
    raise Exception.Create('SymbolTable: VarList overflow');
  VarList[VarCount] := info;
  Result := VarCount;
  Inc(VarCount);
end;

function AllocVar(xr, at: Boolean; size, adr: Integer): Integer;
var i: Integer; ok: Boolean;
begin
  if at then
  begin
    Result := adr; Exit;
  end;
  // naive allocation: find first free gap after existing addresses
  // We'll search increasing addresses from 0 upward
  Result := 0;
  ok := False;
  while not ok do
  begin
    // check overlap
    if not IsVarAtAdr(Result, size) then
    begin
      ok := True; Break;
    end;
    Inc(Result);
    if Result > 10000 then raise Exception.Create('AllocVar: no space');
  end;
end;

// New overloaded function with out parameter
function IsVarAtAdr(adr, size: Integer; out foundIdx: Integer): Boolean;
var i: Integer;
begin
  Result := False; foundIdx := -1;
  if adr = -1 then Exit;
  for i := 0 to VarCount - 1 do
  begin
    if (adr + size > VarList[i].Address) and (adr < VarList[i].Address + VarList[i].Size) then
    begin
      Result := True; foundIdx := i; Exit;
    end;
  end;
end;

// Backwards-compatible two-argument wrapper
function IsVarAtAdr(adr, size: Integer): Boolean;
var idx: Integer;
begin
  Result := IsVarAtAdr(adr, size, idx);
end;

procedure RemoveLocalVars(procIndex: Integer);
var i, j: Integer;
begin
  i := 0;
  while i < VarCount do
  begin
    if VarList[i].Local and (VarList[i].LocProc = procIndex) then
    begin
      for j := i to VarCount - 2 do
        VarList[j] := VarList[j+1];
      Dec(VarCount);
      // do not inc i so we re-examine the moved item
    end else
      Inc(i);
  end;
end;

// Proc API implementations
function GetProcCount: Integer;
begin
  Result := ProcCount;
end;

function GetProcInfo(i: Integer): TProcInfo;
begin
  if (i < 0) or (i >= ProcCount) then
    raise Exception.Create('GetProcInfo: index out of range');
  Result := ProcList[i];
end;

function FindProc(const name: string; out idx: Integer): Boolean;
var i: Integer;
begin
  Result := False; idx := -1;
  for i := 0 to ProcCount - 1 do
  begin
    if SameText(ProcList[i].ProcName, name) then
    begin
      idx := i; Result := True; Exit;
    end;
  end;
end;

function AddProc(const info: TProcInfo): Integer;
begin
  if ProcCount >= MAX_PROCS then
    raise Exception.Create('SymbolTable: ProcList overflow');
  ProcList[ProcCount] := info;
  Result := ProcCount;
  Inc(ProcCount);
end;

end.
