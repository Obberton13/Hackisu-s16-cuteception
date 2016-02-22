"use strict"

const fs = require('fs');
const http = require('http');
const https = require('https');
const url = require('url');

const configPath = '/Users/Nick/scraperConfig.json'
let config = JSON.parse(fs.readFileSync(configPath));


function getSubreditInfo(subreddit, page, callback) {
	var options = url.parse('https://api.imgur.com/3/gallery/r/' + subreddit + '/top/all/' + page + '?perPage=100');
	options.headers = { Authorization: "Client-ID " + config.client_id };

	https.get(options, (res) => {
		res.setEncoding('utf8');
		let data = '';

		res.on('data', (dat) => data += dat);

		res.on('end', () => {
			callback(JSON.parse(data))});
	})
}

function downloadAll(subredditInfo, callback) {
	const NUM_DOWNLOADS = 6;
	let downloadQueue = subredditInfo.data;
	
	for (let i = 0; i < NUM_DOWNLOADS && i < downloadQueue.length; i++) {
		downloadImage(downloadQueue.shift());
	}

	function downloadImage(image) {
		if (image.type == "image/gif") {
			if (downloadQueue.length > 0)
				downloadImage(downloadQueue.shift());
			else (callback());	
		}

		http.get(image.link, (res) => {
			let extension = image.type == "image/png" ? 'png' : 'jpg';
			let path = config.location + '/' + image.id + '.' + extension;
			let data = '';
			res.setEncoding('binary');


			res.on('data', (dat) => data += dat)
				.on('end', () => {
					if (res.statusCode != 200) {
						return;
					}

					let file = fs.openSync(path, 'w');
					fs.writeFileSync(path, data, {encoding: 'binary'});
					fs.closeSync(file);

					if (downloadQueue.length > 0)
						downloadImage(downloadQueue.shift());
					else (callback());
				})
		})
	}
}

function persistConfig() {
	fs.writeFileSync(configPath, JSON.stringify(config));
}

let i = 0;
function getNextSubName() {
	let keys = Object.keys(config.subreddits);
	return keys[(i++) % keys.length];
}

function processSubreddit(subredditName, callback) {
	let subreddits = config.subreddits;

	let subreddit = subreddits[subredditName];

	getSubreditInfo(subredditName, subreddit.page, (info) => {
		if (!info.success) {
			subreddit.state = 'error';
			persistConfig();
			return;
		}

		downloadAll(info, () => {
			console.log('everything downloaded');
			subreddit.page++;
			if (info.data.images_count != 100)
				subreddit.state = 'done'
			else
				subreddit.state = 'running'

			persistConfig();
			callback();
		})
	})
}

function lop() {
	processSubreddit(getNextSubName(), lop);
}

lop();