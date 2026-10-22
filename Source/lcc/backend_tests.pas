program BackendTests;

{$IFDEF FPC}
  {$MODE Delphi}
{$ENDIF}

uses Backend, SysUtils;

var
  TestCount: integer;

function NormalizeNewlines(const Value: string): string;
var
  i: integer;
begin
  Result := '';
  i := 1;
  while i <= Length(Value) do
  begin
    if Value[i] = #13 then
    begin
      Result := Result + #10;
      if (i < Length(Value)) and (Value[i + 1] = #10) then
        Inc(i);
    end
    else
      Result := Result + Value[i];
    Inc(i);
  end;
end;

procedure AssertEqual(const TestName, Actual, Expected: string);
var
  NormalizedActual, NormalizedExpected: string;
begin
  Inc(TestCount);
  NormalizedActual := NormalizeNewlines(Actual);
  NormalizedExpected := NormalizeNewlines(Expected);
  if NormalizedActual <> NormalizedExpected then
  begin
    WriteLn(StdErr, 'FAIL: ', TestName);
    WriteLn(StdErr, 'Expected: ', QuotedStr(NormalizedExpected));
    WriteLn(StdErr, 'Actual:   ', QuotedStr(NormalizedActual));
    Halt(1);
  end;
end;

procedure AssertContains(const TestName, Value, Substring: string);
begin
  Inc(TestCount);
  if Pos(Substring, Value) = 0 then
  begin
    WriteLn(StdErr, 'FAIL: ', TestName, ' (missing ', QuotedStr(Substring), ')');
    Halt(1);
  end;
end;

procedure TestDoubleEXAB;
begin
  AssertEqual(
    'double EXAB pair is removed',
    OptimizeDoubleEXAB(#9 + 'EXAB' + #13#10 + #9 + 'EXAB' + #13#10),
    '');
  AssertEqual(
    'single EXAB and adjacent comment are preserved',
    OptimizeDoubleEXAB(#9 + 'EXAB' + #13#10 + '; keep this comment' + #13#10),
    #9 + 'EXAB' + #13#10 + '; keep this comment' + #13#10);
end;

procedure TestLIAtoLIB;
begin
  AssertEqual(
    'LIA followed by EXAB becomes LIB',
    OptimizeLIAtoLIB(#9 + 'LIA' + #9 + '7' + #9 + '; keep operand comment' +
      #13#10 + #9 + 'EXAB' + #13#10),
    #9 + 'LIB' + #9 + '7' + #9 + '; keep operand comment' + #13#10);
  AssertEqual(
    'LIA without EXAB is preserved',
    OptimizeLIAtoLIB(#9 + 'LIA' + #9 + '7' + #13#10 + #9 + 'LP' + #9 +
      '3' + #13#10),
    #9 + 'LIA' + #9 + '7' + #13#10 + #9 + 'LP' + #9 + '3' + #13#10);
end;

procedure TestRedundantPUSHPOP;
begin
  AssertEqual(
    'PUSH LIB POP is reduced to LIB',
    OptimizeRedundantPUSHPOP(#9 + 'PUSH' + #13#10 + #9 + 'LIB' + #9 +
      '4' + #13#10 + #9 + 'POP' + #13#10),
    #9 + 'LIB' + #9 + '4' + #13#10);
  AssertEqual(
    'PUSH and LIB without POP are preserved',
    OptimizeRedundantPUSHPOP(#9 + 'PUSH' + #13#10 + #9 + 'LIB' + #9 +
      '4' + #13#10 + #9 + 'NOP' + #13#10),
    #9 + 'PUSH' + #13#10 + #9 + 'LIB' + #9 + '4' + #13#10 + #9 + 'NOP' +
      #13#10);
end;

procedure TestIncDec;
begin
  AssertEqual(
    'increment pattern becomes INCA',
    OptimizeIncDec(#9 + 'LIB' + #9 + '1' + #13#10 + #9 + 'LP' + #9 + '3' +
      #13#10 + #9 + 'ADM' + #13#10 + #9 + 'EXAB' + #13#10),
    #9 + 'INCA' + #13#10);
  AssertEqual(
    'increment near-match is preserved',
    OptimizeIncDec(#9 + 'LIB' + #9 + '1' + #13#10 + #9 + 'LP' + #9 + '3' +
      #13#10 + #9 + 'ADW' + #13#10 + #9 + 'EXAB' + #13#10),
    #9 + 'LIB' + #9 + '1' + #13#10 + #9 + 'LP' + #9 + '3' + #13#10 +
      #9 + 'ADW' + #13#10 + #9 + 'EXAB' + #13#10);
  AssertEqual(
    'decrement pattern becomes DECA',
    OptimizeIncDec(#9 + 'LIB' + #9 + '1' + #13#10 + #9 + 'LP' + #9 + '3' +
      #13#10 + #9 + 'EXAB' + #13#10 + #9 + 'SBM' + #13#10 + #9 + 'EXAB' +
      #13#10),
    #9 + 'DECA' + #13#10);
  AssertEqual(
    'decrement near-match is preserved',
    OptimizeIncDec(#9 + 'LIB' + #9 + '1' + #13#10 + #9 + 'LP' + #9 + '3' +
      #13#10 + #9 + 'EXAB' + #13#10 + #9 + 'ADM' + #13#10 + #9 + 'EXAB' +
      #13#10),
    #9 + 'LIB' + #9 + '1' + #13#10 + #9 + 'LP' + #9 + '3' + #13#10 +
      #9 + 'EXAB' + #13#10 + #9 + 'ADM' + #13#10 + #9 + 'EXAB' + #13#10);
end;

procedure TestPipelinePreservesUnrelatedText;
var
  Assembly, Libraries, Optimized: string;
  LineCount: integer;
begin
  Assembly := 'START:' + #13#10 + '; keep assembly comment' + #13#10 +
    #9 + 'LIA' + #9 + '9' + #13#10 + #9 + 'LP' + #9 + '4' + #13#10;
  Libraries := 'LIBRARY_LABEL:' + #13#10 + '; keep library comment' +
    #13#10 + #9 + 'LIB' + #9 + '9';

  Optimized := OptimizeAssembly(Assembly, Libraries, LineCount);
  AssertContains('pipeline preserves label', Optimized, 'START:');
  AssertContains('pipeline preserves assembly comment', Optimized,
    '; keep assembly comment');
  AssertContains('pipeline preserves unrelated LIA', Optimized,
    #9 + 'LIA' + #9 + '9');
  AssertContains('pipeline preserves library label', Optimized, 'LIBRARY_LABEL:');
  AssertContains('pipeline preserves library comment', Optimized,
    '; keep library comment');
  AssertEqual('pipeline reports non-empty output line count',
    IntToStr(LineCount), '7');
end;

begin
  TestDoubleEXAB;
  TestLIAtoLIB;
  TestRedundantPUSHPOP;
  TestIncDec;
  TestPipelinePreservesUnrelatedText;
  WriteLn('PASS: ', TestCount, ' backend optimizer checks.');
end.
