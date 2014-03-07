/*
 * Sebastian Sledz (C) 2014
 * http://garaz.net
 * mailto:sebasledz@gmail.com
 */

function updateAlphabet()
{
	var text = $('#text').val();
	console.log (text);
	var allLetters = getLettersUsedInText(text);
	$('#alphabet').val(allLetters);
}

function getLettersUsedInText(text)
{
	var letters = {};
	for (var i = 0; i < text.length; ++i)
	{
		var c = text.charAt(i);
		letters[c] = true;
	}
	var usedLetters = new Array();
	for (var key in letters)
	{
		usedLetters.push (key);
	}
	for (var i = 1; i < usedLetters.length; ++i)
	{
		var a = usedLetters[i-1];
		var b = usedLetters[i];
		if (a > b)
		{
			usedLetters[i-1] = b;
			usedLetters[i] = a;
			i = 0;
		}
	}
	var output = "";
	for (var i = 0; i < usedLetters.length; ++i)
	{
		var letter = usedLetters[i];
		if (letter != '\r' && letter != '\n' && letter != '\t')
			output = output + letter;
	}
	console.log(output);
	return output;
}
