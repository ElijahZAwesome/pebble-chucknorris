Pebble.addEventListener("ready",function(e) {
	fetchQuote();
});

Pebble.addEventListener("appmessage",function(e) {
	if(e.payload.quote) {
		fetchQuote()
	}
});

function fetchQuote(){
	var req = new XMLHttpRequest();
	req.open('GET', 'http://api.icndb.com/jokes/random', true);
	req.onload = function(e) {
		if (req.readyState == 4 && req.status == 200) {
			var json = JSON.parse(req.responseText);
			if(json['type'] == 'success'){
				var joke = json['value']['joke'];
				console.log("JOKE before : " + joke);
				joke = joke.split('&quot;').join("'");
				console.log("JOKE after : " + joke);
				if(joke.length > 110)
					fetchQuote();
				else {
					sendQuote(joke, 0, Math.round(joke.length / 100));
				}
			}
		} else { 
			console.log("Error " + req.status); 
		}
	}
	req.send(null);
}

function sendQuote(quote, part, total){
	var jokepart = quote.substr(part * 100, 100);
	console.log('part : ' + part + ' ' + total + ' ' + jokepart);
	Pebble.sendAppMessage(
		{'quote':[part, total, jokepart, 0]},
		function(e) {
		  console.log("Successfully delivered message with transactionId=" + e.data.transactionId);
		  if(part < total){
		  	sendQuote(quote, part + 1, total);
		  }
		},
		function(e) {
		  console.log("Unable to deliver message with transactionId=" + e.data.transactionId + " Error is: " + e.error.message);
		});
}
