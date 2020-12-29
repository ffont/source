#include "FreesoundAPI.h"

String URIS::HOST = String("www.freesound.org");
String URIS::BASE = String("https://" + HOST + "/apiv2");
String URIS::TEXT_SEARCH = String("/search/text/");
String URIS::CONTENT_SEARCH = String("/search/content/");
String URIS::COMBINED_SEARCH = String("/search/combined/");
String URIS::SOUND = String("/sounds/<sound_id>/");
String URIS::SOUND_ANALYSIS = String("/sounds/<sound_id>/analysis/");
String URIS::SIMILAR_SOUNDS = String("/sounds/<sound_id>/similar/");
String URIS::COMMENTS = String("/sounds/<sound_id>/comments/");
String URIS::DOWNLOAD = String("/sounds/<sound_id>/download/");
String URIS::UPLOAD = String("/sounds/upload/");
String URIS::DESCRIBE = String("/sounds/<sound_id>/describe/");
String URIS::EDIT = String("/sounds/<sound_id>/edit/");
String URIS::PENDING = String("/sounds/pending_uploads/");
String URIS::BOOKMARK = String("/sounds/<sound_id>/bookmark/");
String URIS::RATE = String("/sounds/<sound_id>/rate/");
String URIS::COMMENT = String("/sounds/<sound_id>/comment/");
String URIS::AUTHORIZE = String("/oauth2/authorize/");
String URIS::LOGOUT = String("/api-auth/logout/");
String URIS::LOGOUT_AUTHORIZE = String("/oauth2/logout_and_authorize/");
String URIS::ACCESS_TOKEN = String("/oauth2/access_token/");
String URIS::ME = String("/me/");
String URIS::USER = String("/users/<username>/");
String URIS::USER_SOUNDS = String("/users/<username>/sounds/");
String URIS::USER_PACKS = String("/users/<username>/packs/");
String URIS::USER_BOOKMARK_CATEGORIES = String("/users/<username>/bookmark_categories/");
String URIS::USER_BOOKMARK_CATEGORY_SOUNDS = String("/users/<username>/bookmark_categories/<category_id>/sounds/");
String URIS::PACK = String("/packs/<pack_id>/");
String URIS::PACK_SOUNDS = String("/packs/<pack_id>/sounds/");
String URIS::PACK_DOWNLOAD = String("/packs/<pack_id>/download/");
String URIS::CONFIRMATION = String("https://" + HOST + "/home/app_permissions/permission_granted/");

URL URIS::uri(String uri, StringArray replacements)
{
	if (!replacements.isEmpty()) {
		for (int i = 0; i < replacements.size(); i++) {
			int start = uri.indexOfChar('<');
			int end = uri.indexOfChar('>');
			uri = uri.replaceSection(start, 1 + end - start, replacements[i]);
		}
	}
	return BASE + uri;	
}

FreesoundClient::FreesoundClient()
{
	token = String();
	clientID = String();
	clientSecret = String();
	header = String();
	auth = None;
}

FreesoundClient::FreesoundClient(String secret)
{
	clientSecret = secret;
	header = "Token " + clientSecret;
	auth = Token;
}

FreesoundClient::FreesoundClient(String id, String secret)
{
	clientID = id;
	clientSecret = secret;
	auth = OAuth;
}

//Function for doing the authorization, the mode selects between LOGOUT_AUTHORIZE(0) and AUTHORIZE(1)
void FreesoundClient::authenticationOnBrowser(int mode, Callback cb)
{
	URL url;
	if(mode == 0){url = URIS::uri(URIS::LOGOUT_AUTHORIZE, StringArray());}
	else{url = URIS::uri(URIS::AUTHORIZE, StringArray()); }
	url = url.withParameter("client_id", clientID);
	url = url.withParameter("response_type", "code");
	url.launchInDefaultBrowser();
	cb();
}

void FreesoundClient::exchangeToken(String authCode, Callback cb)
{
	StringPairArray params;
	params.set("client_id", clientID);
	params.set("client_secret", clientSecret);
	params.set("grant_type", "authorization_code");
	params.set("code", authCode);

	URL url = URIS::uri(URIS::ACCESS_TOKEN, StringArray());
	FSRequest request(url, *this);
	Response resp = request.request(params);
	int resultCode = resp.first;
	if (resultCode == 200) {
		var response = resp.second;
		accessToken = response["access_token"];
		refreshToken = response["refresh_token"];
		header = "Bearer " + accessToken;
	}
	cb();
}

void FreesoundClient::refreshAccessToken(Callback cb) {
	StringPairArray params;
	params.set("client_id", clientID);
	params.set("client_secret", clientSecret);
	params.set("grant_type", "refresh_token");
	params.set("refresh_token", refreshToken);

	URL url = URIS::uri(URIS::ACCESS_TOKEN, StringArray());
	FSRequest request(url, *this);
	Response resp = request.request(params);
	int resultCode = resp.first;
	if (resultCode == 200) {
		var response = resp.second;
		accessToken = response["access_token"];
		refreshToken = response["refresh_token"];
		header = "Bearer " + accessToken;
	}
	cb();
}

// https://freesound.org/docs/api/resources_apiv2.html#text-search
SoundList FreesoundClient::textSearch(String query, String filter, String sort, int groupByPack, int page, int pageSize, String fields, String descriptors, int normalized)
{
	StringPairArray params;
	params.set("query", query);
	params.set("sort", sort);
	params.set("group_by_pack", String(groupByPack));

	if (filter.isNotEmpty()) {
		params.set("filter", filter);
	}

	if (page != -1) {
		params.set("page", String(page));
	}

	if (pageSize != -1) {
		params.set("page_size", String(pageSize));
	}

	if (fields.isNotEmpty()) {
		params.set("fields", fields);
	}

	if (descriptors.isNotEmpty()) {
		params.set("descriptors", descriptors);
	}

	if (normalized != 0) {
		params.set("normalized", "1");
	}

	URL url = URIS::uri(URIS::TEXT_SEARCH, StringArray());
	FSRequest request(url, *this);
	Response resp = request.request(params,String(),false);
	int resultCode = resp.first;
	if (resultCode == 200) {
		var response = resp.second;
		SoundList returnedSounds(response);
		return returnedSounds;
	}
	return SoundList();
}

SoundList FreesoundClient::contentSearch(String target, String descriptorsFilter, int page, int pageSize, String fields, String descriptors, int normalized)
{
	StringPairArray params;

	params.set("target", target);

	if (descriptorsFilter.isNotEmpty()) {
		params.set("descriptors_filter", descriptorsFilter);
	}

	if (page != -1) {
		params.set("page", String(page));
	}

	if (pageSize != -1) {
		params.set("page_size", String(pageSize));
	}

	if (fields.isNotEmpty()) {
		params.set("fields", fields);
	}

	if (descriptors.isNotEmpty()) {
		params.set("descriptors", descriptors);
	}

	if (normalized != 0) {
		params.set("normalized", "1");
	}

	URL url = URIS::uri(URIS::CONTENT_SEARCH, StringArray());
	FSRequest request(url, *this);
	Response resp = request.request(params, String(), false);
	int resultCode = resp.first;
	if (resultCode == 200) {
		var response = resp.second;
		SoundList returnedSounds(response);
		return returnedSounds;
	}
	return SoundList();
}

FSList FreesoundClient::fetchNextPage(FSList soundList)
{
	FSRequest request(soundList.getNextPage(), *this);
	Response resp = request.request(StringPairArray(), String(), false);
	int resultCode = resp.first;
	if (resultCode == 200) {
		var response = resp.second;
		FSList returnedSounds(response);
		return returnedSounds;
	}
	return FSList();
}

FSList FreesoundClient::fetchPreviousPage(FSList soundList)
{
	FSRequest request(soundList.getPreviousPage(), *this);
	Response resp = request.request(StringPairArray(), String(), false);
	int resultCode = resp.first;
	if (resultCode == 200) {
		var response = resp.second;
		FSList returnedSounds(response);
		return returnedSounds;
	}
	return FSList();
}

SoundList FreesoundClient::fetchNextPage(SoundList soundList)
{
	FSRequest request(soundList.getNextPage(), *this);
	Response resp = request.request(StringPairArray(), String(), false);
	int resultCode = resp.first;
	if (resultCode == 200) {
		var response = resp.second;
		SoundList returnedSounds(response);
		return returnedSounds;
	}
	return SoundList();
}

SoundList FreesoundClient::fetchPreviousPage(SoundList soundList)
{
	FSRequest request(soundList.getPreviousPage(), *this);
	Response resp = request.request(StringPairArray(), String(), false);
	int resultCode = resp.first;
	if (resultCode == 200) {
		var response = resp.second;
		SoundList returnedSounds(response);
		return returnedSounds;
	}
	return SoundList();
}

FSSound FreesoundClient::getSound(String id, String fields)
{
	URL url = URIS::uri(URIS::SOUND, StringArray(id));
	StringPairArray params;
	if (fields.isNotEmpty()) {
		params.set("fields", fields);
	}
	
	FSRequest request(url, *this);
	Response resp = request.request(params, String(), false);
	int resultCode = resp.first;
	if (resultCode == 200) {
		var response = resp.second;
		FSSound returnedSound(response);
		return returnedSound;
	}

	return FSSound();
}

var FreesoundClient::getSoundAnalysis(String id, String descriptors, int normalized)
{
	StringPairArray params;

	if (descriptors.isNotEmpty()) {
		params.set("descriptors", descriptors);
	}

	if (normalized != 0) {
		params.set("normalized", "1");
	}

	URL url = URIS::uri(URIS::SOUND_ANALYSIS, id);
	FSRequest request(url, *this);
	Response resp = request.request(params, String(), false);
	int resultCode = resp.first;
	if (resultCode == 200) {
		var response = resp.second;
		return response;
	}
	return var();
}

SoundList FreesoundClient::getSimilarSounds(String id, String descriptorsFilter, int page, int pageSize, String fields, String descriptors, int normalized)
{
	StringPairArray params;

	if (descriptorsFilter.isNotEmpty()) {
		params.set("descriptors_filter", descriptorsFilter);
	}

	if (page != -1) {
		params.set("page", String(page));
	}

	if (pageSize != -1) {
		params.set("page_size", String(pageSize));
	}

	if (fields.isNotEmpty()) {
		params.set("fields", fields);
	}

	if (descriptors.isNotEmpty()) {
		params.set("descriptors", descriptors);
	}

	if (normalized != 0) {
		params.set("normalized", "1");
	}

	URL url = URIS::uri(URIS::SIMILAR_SOUNDS, id);
	FSRequest request(url, *this);
	Response resp = request.request(params, String(), false);
	int resultCode = resp.first;
	if (resultCode == 200) {
		var response = resp.second;
		SoundList returnedSounds(response);
		return returnedSounds;
	}
	return SoundList();
}

std::unique_ptr<URL::DownloadTask> FreesoundClient::downloadSound(FSSound sound, const File & location, URL::DownloadTask::Listener * listener)
{
	URL address = sound.getDownload();
	return address.downloadToFile(location, "Authorization: " + header, listener);
}

std::unique_ptr<URL::DownloadTask> FreesoundClient::downloadOGGSoundPreview(FSSound sound, const File & location, URL::DownloadTask::Listener * listener)
{
    URL address = sound.getOGGPreviewURL();
    return address.downloadToFile(location, "", listener);
}

std::unique_ptr<URL::DownloadTask> FreesoundClient::downloadMP3SoundPreview(FSSound sound, const File & location, URL::DownloadTask::Listener * listener)
{
	URL address = sound.getMP3PreviewURL();
	return address.downloadToFile(location, "", listener);
}

int FreesoundClient::uploadSound(const File & fileToUpload, String tags, String description, String name, String license, String pack, String geotag, Callback cb)
{
	StringPairArray params;

	params.set("tags", tags);
	params.set("description", description);
	params.set("license", license);

	if (name.isNotEmpty()) {
		params.set("name", name);
	}

	if (pack.isNotEmpty()) {
		params.set("pack", pack);
	}

	if (geotag.isNotEmpty()) {
		params.set("geotag", geotag);
	}

	URL url = URIS::uri(URIS::UPLOAD);
	url = url.withFileToUpload("audiofile", fileToUpload, "audio/*");
	FSRequest request(url, *this);
	Response resp = request.request(params, String(), false);
	int resultCode = resp.first;
	if (resultCode == 200) {
		var response = resp.second;
		cb();
		return response["id"];
	}
	cb();
	return 0;
}

int FreesoundClient::describeSound(String uploadFilename, String description, String license, String name, String tags, String pack, String geotag)
{
	StringPairArray params;
	params.set("upload_filename", uploadFilename);
	params.set("license", license);
	params.set("description", description);


	if (tags.isNotEmpty()) {
		params.set("tags", tags);
	}

	if (name.isNotEmpty()) {
		params.set("name", name);
	}

	if (pack.isNotEmpty()) {
		params.set("pack", pack);
	}

	if (geotag.isNotEmpty()) {
		params.set("geotag", geotag);
	}

	URL url = URIS::uri(URIS::DESCRIBE);
	FSRequest request(url, *this);
	Response resp = request.request(params, String(), false);
	int resultCode = resp.first;
	if (resultCode == 200) {
	}

	return resp.second["id"];
}

var FreesoundClient::pendingUploads()
{
	URL url = URIS::uri(URIS::PENDING);
	FSRequest request(url, *this);
	Response resp = request.request(StringPairArray(), String(), false);
	int resultCode = resp.first;
	if (resultCode == 200) {
	}

	return resp.second;
}

void FreesoundClient::editSoundDescription(String id, String name, String tags, String description, String license, String pack, String geotag, Callback cb)
{
	StringPairArray params;

	if (tags.isNotEmpty()) {
		params.set("tags", tags);
	}
	
	if (description.isNotEmpty()) {
		params.set("description", description);
	}

	if (license.isNotEmpty()) {
		params.set("license", license);
	}

	if (name.isNotEmpty()) {
		params.set("name", name);
	}

	if (pack.isNotEmpty()) {
		params.set("pack", pack);
	}

	if (geotag.isNotEmpty()) {
		params.set("geotag", geotag);
	}

	URL url = URIS::uri(URIS::EDIT, id);
	FSRequest request(url, *this);
	Response resp = request.request(params, String(), false);
	int resultCode = resp.first;
	if (resultCode == 200) {
	}

	cb();

}

void FreesoundClient::bookmarkSound(String id, String name, String category, Callback cb)
{
	StringPairArray params;

	if (name.isNotEmpty()) {
		params.set("name", name);
	}

	if (category.isNotEmpty()) {
		params.set("category", category);
	}

	URL url = URIS::uri(URIS::BOOKMARK, id);
	FSRequest request(url, *this);
	Response resp = request.request(params, String(), false);
	int resultCode = resp.first;
	if (resultCode == 200) {
	}
	cb();
}

void FreesoundClient::rateSound(String id, int rating, Callback cb)
{
	StringPairArray params;
	params.set("rating", String(rating));

	URL url = URIS::uri(URIS::RATE, id);
	FSRequest request(url, *this);
	Response resp = request.request(params, String(), false);
	int resultCode = resp.first;
	if (resultCode == 200) {
	}

	cb();
}

void FreesoundClient::commentSound(String id, String comment, Callback cb)
{
	StringPairArray params;
	params.set("comment", comment);

	URL url = URIS::uri(URIS::RATE, id);
	FSRequest request(url, *this);
	Response resp = request.request(params, String(), false);
	int resultCode = resp.first;
	if (resultCode == 200) {
	}
	cb();
}

FSUser FreesoundClient::getUser(String user)
{
	URL url = URIS::uri(URIS::USER, StringArray(user));
	FSRequest request(url, *this);
	Response resp = request.request(StringPairArray(), String(), false);
	int resultCode = resp.first;
	if (resultCode == 200) {
		var response = resp.second;
		FSUser returnedUser(response);
		return returnedUser;
	}

	return FSUser();
}

SoundList FreesoundClient::getUserSounds(String username, String descriptorsFilter, int page, int pageSize, String fields, String descriptors, int normalized)
{
	StringPairArray params;

	if (descriptorsFilter.isNotEmpty()) {
		params.set("descriptors_filter", descriptorsFilter);
	}

	if (page != -1) {
		params.set("page", String(page));
	}

	if (pageSize != -1) {
		params.set("page_size", String(pageSize));
	}

	if (fields.isNotEmpty()) {
		params.set("fields", fields);
	}

	if (descriptors.isNotEmpty()) {
		params.set("descriptors", descriptors);
	}

	if (normalized != 0) {
		params.set("normalized", "1");
	}

	URL url = URIS::uri(URIS::USER_SOUNDS, username);
	FSRequest request(url, *this);
	Response resp = request.request(params, String(), false);
	int resultCode = resp.first;
	if (resultCode == 200) {
		var response = resp.second;
		SoundList returnedSounds(response);
		return returnedSounds;
	}
	return SoundList();
}

FSList FreesoundClient::getUserBookmarkCategories(String username)
{
	URL url = URIS::uri(URIS::USER_BOOKMARK_CATEGORIES, username);
	FSRequest request(url, *this);
	Response resp = request.request(StringPairArray(), String(), false);
	int resultCode = resp.first;
	if (resultCode == 200) {
		var response = resp.second;
		FSList returnedSounds(response);
		return returnedSounds;
	}
	return FSList();
}

FSList FreesoundClient::getUserBookmarkCategoriesSounds(String username, String bookmarkCategory)
{	
	StringArray params;
	params.add(username);
	params.add(bookmarkCategory);
	URL url = URIS::uri(URIS::USER_BOOKMARK_CATEGORY_SOUNDS, params);
	FSRequest request(url, *this);
	Response resp = request.request(StringPairArray(), String(), false);
	int resultCode = resp.first;
	if (resultCode == 200) {
		var response = resp.second;
		FSList returnedSounds(response);
		return returnedSounds;
	}
	return FSList();
}

FSList FreesoundClient::getUserPacks(String username)
{
	URL url = URIS::uri(URIS::USER_PACKS, username);
	FSRequest request(url, *this);
	Response resp = request.request(StringPairArray(), String(), false);
	int resultCode = resp.first;
	if (resultCode == 200) {
		var response = resp.second;
		FSList returnedSounds(response);
		return returnedSounds;
	}
	return FSList();
}

FSPack FreesoundClient::getPack(String id)
{
	URL url = URIS::uri(URIS::PACK, StringArray(id));
	FSRequest request(url, *this);
	Response resp = request.request(StringPairArray(), String(), false);
	int resultCode = resp.first;
	if (resultCode == 200) {
		var response = resp.second;
		FSPack returnedUser(response);
		return returnedUser;
	}

	return FSPack();
}

SoundList FreesoundClient::getPackSounds(String id, String descriptorsFilter, int page, int pageSize, String fields, String descriptors, int normalized)
{
	StringPairArray params;

	if (descriptorsFilter.isNotEmpty()) {
		params.set("descriptors_filter", descriptorsFilter);
	}

	if (page != -1) {
		params.set("page", String(page));
	}

	if (pageSize != -1) {
		params.set("page_size", String(pageSize));
	}

	if (fields.isNotEmpty()) {
		params.set("fields", fields);
	}

	if (descriptors.isNotEmpty()) {
		params.set("descriptors", descriptors);
	}

	if (normalized != 0) {
		params.set("normalized", "1");
	}

	URL url = URIS::uri(URIS::PACK_SOUNDS, StringArray(id));
	FSRequest request(url, *this);
	Response resp = request.request(params, String(), false);
	int resultCode = resp.first;
	if (resultCode == 200) {
		var response = resp.second;
		SoundList returnedSounds(response);
		return returnedSounds;
	}
	return SoundList();
}

std::unique_ptr<URL::DownloadTask> FreesoundClient::downloadPack(FSPack pack, const File & location, URL::DownloadTask::Listener * listener)
{

	URL address = URIS::uri(URIS::PACK_DOWNLOAD, StringArray(pack.getID()));
	return address.downloadToFile(location, this->header, listener);
	
}

FSUser FreesoundClient::getMe()
{
	URL url = URIS::uri(URIS::ME, StringArray());
	FSRequest request(url, *this);
	Response resp = request.request(StringPairArray(), String(), false);
	int resultCode = resp.first;
	if (resultCode == 200) {
		var response = resp.second;
		FSUser returnedUser(response);
		return returnedUser;
	}

	return FSUser();
}


bool FreesoundClient::isTokenNotEmpty()
{
	if (header.isNotEmpty()) { return true; }
	else { return false; }
}

String FreesoundClient::getToken()
{
	return token;
}

String FreesoundClient::getHeader()
{
	return header;
}

String FreesoundClient::getClientID()
{
	return clientID;
}

Response FSRequest::request(StringPairArray params, String data, bool postLikeRequest)
{

	URL url = uri;
	String header;
	int statusCode = -1;
	StringPairArray responseHeaders;
	if (data.isNotEmpty()) { url = url.withPOSTData(data); }
	if (params.size() != 0) { url = url.withParameters(params); }
	if (client.isTokenNotEmpty()) { header = "Authorization: " + client.getHeader(); }


	//Try to open a stream with this information.
	if (auto stream = std::unique_ptr<InputStream>(url.createInputStream(postLikeRequest, nullptr, nullptr, header,
		FREESOUND_API_REQUEST_TIMEOUT, // timeout in millisecs
		&responseHeaders, &statusCode)))
	{
		//Stream created successfully, store the response, log it and return the response in a pair containing (statusCode, response)
		String resp = stream->readEntireStreamAsString();
		var response = JSON::parse(resp);
		return Response(statusCode, response);
	}
	//Couldnt create stream, return (-1, emptyVar)
	return Response(statusCode, var());
}

FSList::FSList()
{
	count = 0;
}

FSList::FSList(var response)
{
	count = response["count"]; //getIntValue
	nextPage = response["next"];
	previousPage = response["previous"];
	results = response["results"];
}

String FSList::getNextPage()
{
	return nextPage;
}

String FSList::getPreviousPage()
{
	return previousPage;
}

var FSList::getResults()
{
	return results;
}

int FSList::getCount()
{
	return count;
}

FSSound::FSSound()
{
}

FSSound::FSSound(var sound)
{
	id = sound["id"];
	url = URL(sound["url"]);
	name = sound["name"];
	tags = StringArray();
	//Verificar que isto está bem. sound[tags]precisa de ser um array
	for (int i = 0; i < sound["tags"].size(); i++) {
		tags.add(sound["tags"][i]);
	}
	description = sound["description"];
	geotag = sound["geotag"];
	created = sound["created"];
	license = sound["license"];
	format = sound["type"];
	channels = sound["channels"];
	filesize = sound["filesize"];
	bitrate = sound["bitrate"];
	bitdepth = sound["bitdepth"];
	duration = sound["duration"];
	samplerate = sound["samplerate"];
	user = sound["username"];
	pack = URL(sound["pack"]);
	download = URL(sound["download"]);
	bookmark = URL(sound["bookmark"]);
	previews = sound["previews"];
    images = sound["images"];
	numDownloads = sound["images"];
	avgRating = sound["num_downloads"];
	numRatings = sound["avg_rating"];
	rate = URL(sound["rate"]);
	comments = URL(sound["comments"]);
	numComments = sound["num_comments"];
	comment = URL(sound["comment"]);
	similarSounds = URL(sound["similar_sounds"]);
	analysis = sound["analysis"];
	analysisStats = URL(sound["analysis_stats"]);
	analysisFrames = URL(sound["analysis_frames"]);
	acAnalysis = sound["ac_analysis"];

}

URL FSSound::getDownload()
{
	return download;
}

URL FSSound::getOGGPreviewURL()
{
    if (!previews.hasProperty("preview-hq-ogg")){
        /*
         If you reached this bit of code is because you're trying to download the preview of a sound
         but the freesound-juce client does not know the URL where to find that preview.
         
         It can happen that the FSSound object does not have "previews" property if the object was
         constructed from the results of a SoundList (e.g. using SoundList::toSoundArray method) and
         the "previews" field was not initially requested in the query that created the SoundList
         object. If you just reached this bit of code, make sure that you include the "previews" field
         in the "fields" parameter of the function that generated the SoundList.
         */
        throw;
    }
    return URL(previews["preview-hq-ogg"]);
}

URL FSSound::getMP3PreviewURL()
{
	if (!previews.hasProperty("preview-hq-mp3")) {
		/*
		 If you reached this bit of code is because you're trying to download the preview of a sound
		 but the freesound-juce client does not know the URL where to find that preview.

		 It can happen that the FSSound object does not have "previews" property if the object was
		 constructed from the results of a SoundList (e.g. using SoundList::toSoundArray method) and
		 the "previews" field was not initially requested in the query that created the SoundList
		 object. If you just reached this bit of code, make sure that you include the "previews" field
		 in the "fields" parameter of the function that generated the SoundList.
		 */
		throw;
	}
	return URL(previews["preview-hq-mp3"]);
}

FSUser::FSUser()
{
}

FSUser::FSUser(var user)
{
	profile = URL(user["url"]);
	about = user["about"];
	username = user["username"];
	homepage = URL(user["homepage"]);
	avatar = user["avatar"];
	dateJoined = user["date_joined"];
	numSounds = user["num_sounds"];
	sounds = URL(user["sounds"]);
	numPacks = user["num_packs"];
	packs = URL(user["packs"]);
	numPosts = user["num_posts"];
	numComments = user["num_comments"];
	bookmarks = URL(user["bookmarks"]);

}

FSPack::FSPack()
{
}

FSPack::FSPack(var pack)
{
	id = pack["id"];
	url = URL(pack["url"]);
	description = pack["description"];
	created = pack["created"];
	name = pack["name"];
	username = pack["username"];
	numSounds = pack["num_sounds"];
	sounds = URL(pack["sounds"]);
	numDownloads = pack["num_downloads"];
}

String FSPack::getID()
{
	return id;
}

Array<FSSound> SoundList::toArrayOfSounds()
{


	Array<FSSound> arrayOfSounds;

	for (int i = 0; i < results.size(); i++)
	{
		arrayOfSounds.add(FSSound(results[i]));
	}

	return arrayOfSounds;
}
