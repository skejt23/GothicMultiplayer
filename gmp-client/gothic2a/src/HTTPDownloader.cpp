#include "HTTPDownloader.h"

#include <httplib.h>

using namespace std;
using namespace httplib;

string HTTPDownloader::GetContentOfHost(string address, string path)
{
	Client client("http://" + address);
	auto response = client.Get("/" + path);
	return response->body;
}

string HTTPDownloader::GetWBFile(string serverAddress)
{
	static const string wbFilePath = "wb_file";
	return GetContentOfHost(serverAddress, wbFilePath);
}

string HTTPDownloader::GetClassFile(string serverAddress)
{
	static const string classFilePath = "class";
	return GetContentOfHost(serverAddress, classFilePath);
}

string HTTPDownloader::GetSpawnpointsFile(string serverAddress)
{
	static const string spawnpointFilePath = "spawnpoint";
	return GetContentOfHost(serverAddress, spawnpointFilePath);
}