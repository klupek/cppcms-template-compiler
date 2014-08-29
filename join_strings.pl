local $/ = undef; 
local $string = <>; 
while($string =~ m/"(\\"|[^"])+[^\\]"\s+"/) { 
	$string =~ s/(^|[^\\])("(\\"|[^"])+[^\\])"\s+"/$1$2/gsm; 
} 

print $string
