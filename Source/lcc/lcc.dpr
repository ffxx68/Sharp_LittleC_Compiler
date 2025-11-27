{--------------------------------------------------------------}
program lcc;

{$IFDEF FPC}
  {$MODE Delphi}
{$ENDIF}

{$APPTYPE CONSOLE}
uses
  SysUtils, Input, Output, Errors, Scanner, Parser, CodeGen, CalcUnit, Lexer;

begin
        outfile := true;
        writeln;
        writeln('lcc v2.1 - littleC Compiler for Hitachi SC61860 CPU');
        writeln('(c) Simon Lehmayr 2004');
        writeln('(c) Fabio Fumi - refactored - 2025');
        if paramcount = 2 then
        begin
                if not fileexists(ParamStr(1)) then exit;
                writeln('Compiling ',paramstr(1),'...');
                FirstScan(ParamStr(1));
                SecondScan(ParamStr(2));
        end else
        begin
                writeln('Usage: lcc inputfile outputfile');
                writeln('    input file must be the file the preprocessor created');
                writeln('    output file must be the assembler file to create');
        end;
end.
