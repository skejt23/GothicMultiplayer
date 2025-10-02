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