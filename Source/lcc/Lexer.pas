unit Lexer;
interface
{$IFDEF FPC}
  {$MODE Delphi}
  {$H+}
{$ENDIF}
uses SysUtils, Input, Errors;

procedure InitLexerFromFile(const filename: string);
procedure InitLexerFromString(const s: string);
procedure GetToken(mode: Integer; var s: string);
function GetName: string;
function GetNumber: string;
function GetFloat: string;
function CopyToken(s: string): string;
function CurrentToken: string;
function CurrentLine: Integer;

implementation
uses Scanner;

procedure InitLexerFromFile(const filename: string);
begin
  // Wrapper for scanner/init from file
  // ...existing code...
end;

procedure InitLexerFromString(const s: string);
begin
  // Wrapper for scanner/init from string
  // ...existing code...
end;

procedure GetToken(mode: Integer; var s: string);
begin
  Scanner.GetToken(mode, s);
end;

function GetName: string;
begin
  GetName := Scanner.GetName;
end;

function GetNumber: string;
begin
  GetNumber := Scanner.GetNumber;
end;

function GetFloat: string;
begin
  GetFloat := Scanner.GetFloat;
end;

function CopyToken(s: string): string;
begin
  CopyToken := Scanner.CopyToken(s);
end;

function CurrentToken: string;
begin
  CurrentToken := Scanner.Tok;
end;

function CurrentLine: Integer;
begin
  CurrentLine := Scanner.linecnt;
end;

end.
