unit Backend;

{$IFDEF FPC}
  {$MODE Delphi}
{$ENDIF}

interface

function OptimizeDoubleEXAB(const Assembly: string): string;
function OptimizeLIAtoLIB(const Assembly: string): string;
function OptimizeRedundantPUSHPOP(const Assembly: string): string;
function OptimizeIncDec(const Assembly: string): string;
function OptimizeAssembly(const Assembly, Libraries: string;
  out LineCount: integer): string;

implementation

uses Classes, SysUtils;

const
  CRLF = #13#10;

function HasPrefix(const Line, Prefix: string): boolean;
begin
  Result := Copy(Trim(Line), 1, Length(Prefix)) = Prefix;
end;

function OptimizeInitialIncDec(const Assembly: string): string;
var
  i, p: integer;
  s, s2, s3: string;

  procedure ReplaceMatches(const Pattern: string; var Replacement: string;
    const Suffix: string);
  begin
    p := Pos(Pattern, Result);
    while p > 0 do
    begin
      Delete(Result, p, Length(Pattern));
      Insert(Replacement, Result, p);
      Replacement := Replacement + Suffix;
      p := Pos(Pattern, Result);
    end;
  end;

begin
  Result := Assembly;
  s2 := #9 + 'INCA';
  s3 := #9 + 'DECA';
  for i := 1 to 6 do
  begin
    s := #9 + 'LIA' + #9 + IntToStr(i) + #9 + '; Load constant ' +
      IntToStr(i) + CRLF + #9 + 'EXAB' + CRLF + #9 + 'LP' + #9 + '3' +
      CRLF + #9 + 'SBM' + #9 + #9 + '; Subtraction' + CRLF + #9 + 'EXAB';
    ReplaceMatches(s, s2, CRLF + #9 + 'DECA');

    s := #9 + 'LIA' + #9 + IntToStr(i) + #9 + '; Load constant ' +
      IntToStr(i) + CRLF + #9 + 'LP' + #9 + '3' + CRLF + #9 + 'ADM' +
      #9 + #9 + '; Addition' + CRLF + #9 + 'EXAB';
    ReplaceMatches(s, s3, CRLF + #9 + 'INCA');
  end;

  s := #9 + 'EXAB' + CRLF + #9 + 'EXAB' + CRLF;
  while Pos(s, Result) > 0 do
  begin
    p := Pos(s, Result);
    Delete(Result, p, Length(s));
  end;
end;

function OptimizeDoubleEXAB(const Assembly: string): string;
var
  Lines: TStringList;
  i: integer;
begin
  Lines := TStringList.Create;
  try
    Lines.Text := Assembly;
    i := 0;
    while i + 1 < Lines.Count do
    begin
      if HasPrefix(Lines[i], 'EXAB') then
      begin
        if HasPrefix(Lines[i + 1], 'EXAB') then
        begin
          Lines.Delete(i + 1);
          Lines.Delete(i);
        end
        else
          Inc(i, 2);
      end
      else
        Inc(i);
    end;
    Result := Lines.Text;
  finally
    Lines.Free;
  end;
end;

function OptimizeLIAtoLIB(const Assembly: string): string;
var
  Lines: TStringList;
  i: integer;
  NameLine: string;
begin
  Lines := TStringList.Create;
  try
    Lines.Text := Assembly;
    i := 0;
    while i < Lines.Count do
    begin
      if HasPrefix(Lines[i], 'LIA') and (i + 1 < Lines.Count) then
      begin
        if HasPrefix(Lines[i + 1], 'EXAB') then
        begin
          NameLine := Lines[i];
          Lines[i] := #9 + 'LIB' + Copy(NameLine, 5, 255);
          Lines.Delete(i + 1);
          Inc(i);
        end
        else
          Inc(i, 2);
      end
      else
        Inc(i);
    end;
    Result := Lines.Text;
  finally
    Lines.Free;
  end;
end;

function OptimizeRedundantPUSHPOP(const Assembly: string): string;
var
  Lines: TStringList;
  i: integer;
begin
  Lines := TStringList.Create;
  try
    Lines.Text := Assembly;
    i := 0;
    while i < Lines.Count do
    begin
      if HasPrefix(Lines[i], 'PUSH') and (i + 1 < Lines.Count) then
      begin
        if HasPrefix(Lines[i + 1], 'LIB') then
        begin
          if (i + 2 < Lines.Count) and HasPrefix(Lines[i + 2], 'POP') then
          begin
            Lines[i] := Lines[i + 1];
            Lines.Delete(i + 2);
            Lines.Delete(i + 1);
          end
          else
            Inc(i, 3);
        end
        else
          Inc(i, 2);
      end
      else
        Inc(i);
    end;
    Result := Lines.Text;
  finally
    Lines.Free;
  end;
end;

function OptimizeIncDec(const Assembly: string): string;
var
  Lines: TStringList;
  i: integer;
begin
  Lines := TStringList.Create;
  try
    Lines.Text := Assembly;
    i := 0;
    while i < Lines.Count do
    begin
      if (HasPrefix(Trim(Lines[i]), 'LIB'#9'1'#9) or
        (Trim(Lines[i]) = 'LIB'#9'1')) and
        (i + 1 < Lines.Count) then
      begin
        if HasPrefix(Trim(Lines[i + 1]), 'LP'#9'3') and
          (i + 2 < Lines.Count) then
        begin
          if HasPrefix(Lines[i + 2], 'ADM') and (i + 3 < Lines.Count) then
          begin
            if HasPrefix(Lines[i + 3], 'EXAB') then
            begin
              Lines[i] := #9 + 'INCA';
              Lines.Delete(i + 3);
              Lines.Delete(i + 2);
              Lines.Delete(i + 1);
            end
            else
              Inc(i, 4);
          end
          else
            Inc(i, 3);
        end
        else
          Inc(i, 2);
      end
      else
        Inc(i);
    end;

    i := 0;
    while i < Lines.Count do
    begin
      if (HasPrefix(Trim(Lines[i]), 'LIB'#9'1'#9) or
        (Trim(Lines[i]) = 'LIB'#9'1')) and
        (i + 1 < Lines.Count) then
      begin
        if HasPrefix(Trim(Lines[i + 1]), 'LP'#9'3') and
          (i + 2 < Lines.Count) then
        begin
          if HasPrefix(Lines[i + 2], 'EXAB') and (i + 3 < Lines.Count) then
          begin
            if HasPrefix(Lines[i + 3], 'SBM') and (i + 4 < Lines.Count) then
            begin
              if HasPrefix(Lines[i + 4], 'EXAB') then
              begin
                Lines[i] := #9 + 'DECA';
                Lines.Delete(i + 4);
                Lines.Delete(i + 3);
                Lines.Delete(i + 2);
                Lines.Delete(i + 1);
              end
              else
                Inc(i, 5);
            end
            else
              Inc(i, 4);
          end
          else
            Inc(i, 3);
        end
        else
          Inc(i, 2);
      end
      else
        Inc(i);
    end;

    Result := Lines.Text;
  finally
    Lines.Free;
  end;
end;

function OptimizeAssembly(const Assembly, Libraries: string;
  out LineCount: integer): string;
var
  Lines: TStringList;
  i: integer;
begin
  Result := OptimizeInitialIncDec(Assembly);
  Result := Result + CRLF + Libraries + CRLF;
  Result := OptimizeLIAtoLIB(Result);
  Result := OptimizeRedundantPUSHPOP(Result);
  Result := OptimizeDoubleEXAB(Result);
  Result := OptimizeIncDec(Result);

  Lines := TStringList.Create;
  try
    Lines.Text := Result;
    LineCount := 0;
    for i := 0 to Lines.Count - 1 do
      if Lines[i] <> '' then
        Inc(LineCount);
    Result := Lines.Text;
  finally
    Lines.Free;
  end;
end;

end.
