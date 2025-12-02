unit Semantic;

{$IFDEF FPC}
  {$MODE Delphi}
{$ENDIF}

interface

uses SysUtils, SymbolTable, Scanner, Lexer, Errors;

// Variable declaration parsing
function VarDecl: string;

// Procedure local variable address calculation
procedure RepAdr(currproc: Integer);

implementation

{--------------------------------------------------------------}
{ Parse Variable Declaration }
{ Extracts type and variable name(s) from token string and adds them to symbol table }

function VarDecl: string;
var Name, Typ, t: string;
    xr, p, l: boolean;
begin
          Name := ExtrWord(Tok);
          Typ := Name;
          Tok := Tok + ',';
          xr := false;
          repeat
                  t := ExtrList(Tok);
                  Name := trim(ExtrWord(t));
                  p := false;
                  if pos('*', name) = 1 then
                  begin
                          p := true;
                          delete(name, 1, 1); name := trim(name);
                  end;
                  if Name = 'xram' then
                  begin
                          xr := true;
                          Name := ExtrWord(t);
                  end;
                  if t <> '' then
                          Name := Name + ' ' + t;
                  if Level = 0 then l := false else l := true;

                  // Set locproc for local variables
                  if l then
                  begin
                    // We need to set locproc in the VarList before calling AddVar
                    // This is a temporary workaround until full migration
                    // For now, we'll handle this in parser.pas
                  end;

                  result := Name;
                  // Note: AddVar is still called from parser.pas
                  // This function just prepares the variable declaration
          until Tok = '';
end;

{--------------------------------------------------------------}
{ Recalculate Addresses for Local Variables and Parameters }
{ Assigns stack offsets to local variables and parameters of a procedure }

procedure RepAdr(currproc: Integer);
var i, lc, pc, m, a: integer;
    name: string;
    proc: TProcInfo;
    varinfo: TVarInfo;
    idx: Integer;
begin
        proc := SymbolTable.GetProcInfo(currproc);
        lc := proc.loccnt;
        pc := proc.parcnt;

        if (lc = 0) and (pc = 0) then exit;

        if lc > 0 then
                name := proc.locname[lc - 1]
        else
                name := proc.parname[pc - 1];

        if not SymbolTable.FindVar(name, idx) then
                error('Var '+name+' not declared!');

        varinfo := SymbolTable.GetVarInfo(idx);
        if not varinfo.local then
                error('Var '+name+' not local!');

        // sum up local space needed
        m := 0; // counting return address location
        if pc > 0 then for i := 0 to pc - 1 do
                if proc.partyp[i] = 'float' then inc(m, 8)
                else if proc.partyp[i] = 'word' then inc(m, 2)
                else if ((proc.partyp[i] = 'byte') or
                         (proc.partyp[i] = 'char')) then inc(m)
                else Error ('Invalid parameter type <' + proc.partyp[i] + '> in '
                      + proc.ProcName );

        if lc > 0 then for i := 0 to lc - 1 do
                if proc.loctyp[i] = 'float' then inc(m, 8)
                else if proc.loctyp[i] = 'word' then inc(m, 2)
                else if ((proc.loctyp[i] = 'byte') or
                         (proc.loctyp[i] = 'char')) then inc(m)
                else Error ('Invalid local var type <' + proc.loctyp[i] + '> in '
                     + proc.ProcName );

        // calc each local relative address (offset), starting from the end
        a := 1;
        if pc > 0 then for i := 0 to pc - 1 do
        begin
                name := proc.parname[i];
                if not SymbolTable.FindVar(name, idx) then
                        error('Var '+name+' not declared!');

                varinfo := SymbolTable.GetVarInfo(idx);
                if not varinfo.local then
                        error('Var '+name+' not local!');

                // Update address in local VarList (temporary - will be migrated)
                // For now, we need to access the parser's VarList directly
                // This will be fixed in a future refactor
                writeln('LOCAL ',proc.procname,' PARAM: ', name,
                               '(',proc.partyp[i],')',
                               ': ', m - a );

                if proc.partyp[i] = 'float' then inc(a, 8)
                else if proc.partyp[i] = 'word' then inc(a, 2)
                else if ((proc.partyp[i] = 'byte') or
                         (proc.partyp[i] = 'char')) then inc(a);
        end;

        if lc > 0 then for i := 0 to lc - 1 do
        begin
                name := proc.locname[i];
                if not SymbolTable.FindVar(name, idx) then
                        error('Var '+name+' not declared!');

                varinfo := SymbolTable.GetVarInfo(idx);
                if not varinfo.local then
                        error('Var '+name+' not local!');

                // Update address using SymbolTable API
                SymbolTable.SetVarAddress(idx, m - a);
                writeln('LOCAL ',proc.procname,' VAR: ', name,
                               '(',proc.loctyp[i],')',
                               ': ', m - a );

                if proc.loctyp[i] = 'float' then inc(a, 8)
                else if proc.loctyp[i] = 'word' then inc(a, 2)
                else if ((proc.loctyp[i] = 'byte') or
                         (proc.loctyp[i] = 'char')) then inc(a);
        end;
end;

end.

