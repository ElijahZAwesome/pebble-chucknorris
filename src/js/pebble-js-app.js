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
	req.open('GET', 'http://api.icndb.com/jokes/random?escape=javascript', true);
	req.onload = function(e) {
		if (req.readyState == 4 && req.status == 200) {
			var json = JSON.parse(req.responseText);
			if(json['type'] == 'success'){
				var joke = json['value']['joke'];
				if(joke.length > 110)
					fetchQuote();
				else {
					sendQuote(joke);
				}
			}
		} else { 
			console.log("Error " + req.status); 
		}
	}
	req.send(null);
}

function sendQuote(quote){
	console.log('quote : ' +quote);
	Pebble.sendAppMessage(
		{'quote':[quote,0]},
		function(e) {
		  console.log("Successfully delivered message with transactionId=" + e.data.transactionId);
		},
		function(e) {
		  console.log("Unable to deliver message with transactionId=" + e.data.transactionId + " Error is: " + e.error.message);
		});
}
