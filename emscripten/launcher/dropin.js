(() => {
	function extract_filename (str) {
		const sep = str.lastIndexOf('/');
		return sep < 0 ? str : str.substr(sep + 1);
	}

	const params = Object.fromEntries(new URLSearchParams(location.search));
	let filename;

	if (!('ft2c_load_url' in params)) {
		return;
	}

	if ('ft2c_load_filename' in params) {
		filename = params.ft2c_load_filename;
	}
	else {
		filename = extract_filename(params.ft2c_load_url);
	}

	Module.arguments = [ filename ];
	Module.preRun = () => {
		FS.createPreloadedFile('.', filename, params.ft2c_load_url, true, false);
	};
})();