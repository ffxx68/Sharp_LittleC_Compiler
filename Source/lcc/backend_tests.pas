program BackendTests;

{$IFDEF FPC}
  {$MODE Delphi}
{$ENDIF}

uses Backend, CodeGen, Classes, Output, SysUtils;

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

function CountOccurrences(const Value, Substring: string): integer;
var
  p, StartAt: integer;
begin
  Result := 0;
  StartAt := 1;
  repeat
    p := Pos(Substring, Copy(Value, StartAt, MaxInt));
    if p > 0 then
    begin
      Inc(Result);
      Inc(StartAt, p + Length(Substring) - 1);
    end;
  until p = 0;
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
  Assembly, Libraries, Optimized, LibraryRoutine: string;
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

  LibraryRoutine := 'LIB_PATTERN:'#13#10 + #9 + 'LIB'#9'1'#13#10 +
    #9 + 'LP'#9'3'#13#10 + #9 + 'ADM'#13#10 + #9 + 'EXAB';
  Optimized := OptimizeAssembly(#9 + 'CALL'#9'main'#13#10,
    LibraryRoutine, LineCount);
  AssertContains('runtime library text is not optimizer-rewritten', Optimized,
    LibraryRoutine);
end;

procedure TestRuntimeLibraries(const FixturePath: string);
const
  RuntimeLibraries: array[0..15] of string =
    ('divmod8', 'sr8', 'sl8', 'xor8', 'mul8', 'mul16', 'div16', 'xor16',
     'sr16', 'sl16', 'cpe16', 'cpne16', 'cps16', 'cpg16', 'cpse16', 'cpge16');
  ComparisonLibraries: array[0..5] of string =
    ('cpe16', 'cpne16', 'cps16', 'cpg16', 'cpse16', 'cpge16');
var
  i: integer;
  ErrorMessage: string;
  Fixture: TStringList;
  Libraries: string;
begin
  ResetRuntimeLibraries;
  ResolveRuntimeLibraries;
  AssertEqual('no-library program has no runtime code', GetLibText, '');

  asmtext := #9 + 'CALL' + #9 + 'LIB_UNTRACKED';
  ErrorMessage := '';
  try
    ResolveRuntimeLibraries;
  except
    on E: Exception do
      ErrorMessage := E.Message;
  end;
  AssertContains('untracked runtime call is rejected', ErrorMessage,
    'has no included routine definition');
  asmtext := '';

  ResetRuntimeLibraries;
  outfile := True;
  optype := otWord;
  pushcnt := 4;
  CompEqual;
  CompEqual;
  ResolveRuntimeLibraries;
  AssertEqual('repeated codegen helper calls remain in generated code',
    IntToStr(CountOccurrences(asmtext, 'CALL'#9'LIB_CPE16')), '2');
  AssertEqual('codegen deduplicates repeated helper definitions',
    IntToStr(CountOccurrences(GetLibText, 'LIB_CPE16:')), '1');
  asmtext := '';

  ResetRuntimeLibraries;
  RequireRuntimeLibrary('cpe16');
  ResolveRuntimeLibraries;
  Libraries := GetLibText;
  AssertContains('single runtime dependency is embedded', Libraries,
    'LIB_CPE16:');
  AssertEqual('embedded library has no unresolved include',
    IntToStr(CountOccurrences(Libraries, '.include')), '0');

  RequireRuntimeLibrary('cpe16');
  ResolveRuntimeLibraries;
  Libraries := GetLibText;
  AssertEqual('repeated runtime dependency is embedded once',
    IntToStr(CountOccurrences(Libraries, 'LIB_CPE16:')), '1');

  ResetRuntimeLibraries;
  for i := Low(RuntimeLibraries) to High(RuntimeLibraries) do
    RequireRuntimeLibrary(RuntimeLibraries[i]);
  ResolveRuntimeLibraries;
  Libraries := GetLibText;
  for i := Low(ComparisonLibraries) to High(ComparisonLibraries) do
    AssertEqual('comparison helper embedded once: ' + ComparisonLibraries[i],
      IntToStr(CountOccurrences(Libraries,
        'LIB_' + UpperCase(ComparisonLibraries[i]) + ':')), '1');
  AssertContains('byte division helper embedded', Libraries, 'LIB_DIV8:');
  AssertContains('byte modulo helper embedded', Libraries, 'LIB_MOD8:');
  AssertContains('word XOR dependency embedded', Libraries, 'LIB_XOR8:');

  Fixture := TStringList.Create;
  try
    Fixture.Add('.ORG 40000');
    Fixture.Add(#9 + 'CALL' + #9 + 'main');
    Fixture.Add('main:');
    Fixture.Add(#9 + 'CALL' + #9 + 'LIB_CPE16');
    Fixture.Add(#9 + 'RTN');
    Fixture.Add('');
    Fixture.Text := Fixture.Text + Libraries;
    Fixture.SaveToFile(FixturePath);
  finally
    Fixture.Free;
  end;

  ResetRuntimeLibraries;
  RequireRuntimeLibrary('missing_runtime_test');
  ErrorMessage := '';
  try
    ResolveRuntimeLibraries;
  except
    on E: Exception do
      ErrorMessage := E.Message;
  end;
  AssertContains('unresolved runtime dependency reports an error',
    ErrorMessage, 'could not be resolved');
  ResetRuntimeLibraries;
end;

begin
  if ParamCount <> 1 then
  begin
    WriteLn(StdErr, 'Expected path for PASM runtime-library fixture');
    Halt(2);
  end;
  TestDoubleEXAB;
  TestLIAtoLIB;
  TestRedundantPUSHPOP;
  TestIncDec;
  TestPipelinePreservesUnrelatedText;
  TestRuntimeLibraries(ParamStr(1));
  WriteLn('PASS: ', TestCount, ' backend and runtime-library checks.');
end.
