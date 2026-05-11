//{$MODE DELPHI}
{--------------------------------------------------------------}
unit Output;

{$IFDEF FPC}
  {$MODE Delphi}
{$ENDIF}

{--------------------------------------------------------------}
interface
procedure writLn(s: string);		{ Emit an instruction line }
procedure addasm(s: string);

{ Library text management API }
procedure AddLibText(const s: string);
function GetLibText: string;
procedure ClearLibText;

// Write accumulated asm and lib text to a file
procedure SaveToFile(const filename: string);

var
        f: textfile;
        asmtext: string;
        libtext: string;
        asmcnt: integer;
        asmlist: Array [0..10000] of string;
        outfile: boolean;


{--------------------------------------------------------------}
implementation
const TAB = ^I;

{--------------------------------------------------------------}
{ Emit an Instruction, Followed By a Newline }

procedure writln(s: string);
begin
        if outfile then
	    asmtext := asmtext + s + #13#10
        else
            addasm(s);
end;


procedure addasm(s: string);
begin
        asmlist[asmcnt] := s;
        inc(asmcnt);
end;

{--------------------------------------------------------------}
{ Library text management API }

procedure AddLibText(const s: string);
begin
  libtext := libtext + s;
end;

function GetLibText: string;
begin
  Result := libtext;
end;

procedure ClearLibText;
begin
  libtext := '';
end;

procedure SaveToFile(const filename: string);
var
  outF: textfile;
begin
  assignfile(outF, filename);
  rewrite(outF);
  writeln(outF, asmtext);
  writeln(outF, libtext);
  closefile(outF);
end;

begin
        asmtext := '';
        libtext := '';
end.
