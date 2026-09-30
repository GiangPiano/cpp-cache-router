local counter = 1
function setup(thread)
	thread:set("id", counter)
	counter = counter + 1
end

function init(args)
	math.randomseed(os.time() + id)
	keyspace = 400
	read_ratio = 0.9
	headers = { ["Content-Type"] = "application/json" }
	body = '{"value":"123"}'
end

request = function()
	local path = "/api/cache/key-" .. math.random(keyspace)
	if math.random() < read_ratio then
		return wrk.format("GET", path)
	end
	return wrk.format("PUT", path, headers, body)
end
