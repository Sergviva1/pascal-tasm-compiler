program Example;
const
  Limit = 10;
var
  a, b: integer;
  flag: boolean;

procedure Compare(x: integer; var result: boolean);
begin
  if x < Limit then
    result := true
  else
    result := false
end;

begin
  a := 5;
  b := 0;
  Compare(a, flag)
end.