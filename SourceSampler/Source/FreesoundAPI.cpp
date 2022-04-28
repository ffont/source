#include "FreesoundAPI.h"

juce::String URIS::HOST = juce::String("freesound.org");
juce::String URIS::BASE = juce::String("https://" + HOST + "/apiv2");
juce::String URIS::TEXT_SEARCH = juce::String("/search/text/");
juce::String URIS::CONTENT_SEARCH = juce::String("/search/content/");
juce::String URIS::COMBINED_SEARCH = juce::String("/search/combined/");
juce::String URIS::SOUND = juce::String("/sounds/<sound_id>/");
juce::String URIS::SOUND_ANALYSIS = juce::String("/sounds/<sound_id>/analysis/");
juce::String URIS::SIMILAR_SOUNDS = juce::String("/sounds/<sound_id>/similar/");
juce::String URIS::COMMENTS = juce::String("/sounds/<sound_id>/comments/");
juce::String URIS::DOWNLOAD = juce::String("/sounds/<sound_id>/download/");
juce::String URIS::UPLOAD = juce::String("/sounds/upload/");
juce::String URIS::DESCRIBE = juce::String("/sounds/<sound_id>/describe/");
juce::String URIS::EDIT = juce::String("/sounds/<sound_id>/edit/");
juce::String URIS::PENDING = juce::String("/sounds/pending_uploads/");
juce::String URIS::BOOKMARK = juce::String("/sounds/<sound_id>/bookmark/");
juce::String URIS::RATE = juce::String("/sounds/<sound_id>/rate/");
juce::String URIS::COMMENT = juce::String("/sounds/<sound_id>/comment/");
juce::String URIS::AUTHORIZE = juce::String("/oauth2/authorize/");
juce::String URIS::LOGOUT = juce::String("/api-auth/logout/");
juce::String URIS::LOGOUT_AUTHORIZE = juce::String("/oauth2/logout_and_authorize/");
juce::String URIS::ACCESS_TOKEN = juce::String("/oauth2/access_token/");
juce::String URIS::ME = juce::String("/me/");
juce::String URIS::USER = juce::String("/users/<username>/");
juce::String URIS::USER_SOUNDS = juce::String("/users/<username>/sounds/");
juce::String URIS::USER_PACKS = juce::String("/users/<username>/packs/");
juce::String URIS::USER_BOOKMARK_CATEGORIES = juce::String("/users/<username>/bookmark_categories/");
juce::String URIS::USER_BOOKMARK_CATEGORY_SOUNDS = juce::String("/users/<username>/bookmark_categories/<category_id>/sounds/");
juce::String URIS::PACK = juce::String("/packs/<pack_id>/");
juce::String URIS::PACK_SOUNDS = juce::String("/packs/<pack_id>/sounds/");
juce::String URIS::PACK_DOWNLOAD = juce::String("/packs/<pack_id>/download/");
juce::String URIS::CONFIRMATION = juce::String("https://" + HOST + "/home/app_permissions/permission_granted/");

juce::URL URIS::uri(juce::String uri, juce::StringArray replacements)
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
	token = juce::String();
	clientID = juce::String();
	clientSecret = juce::String();
	header = juce::String();
	auth = None;
}

FreesoundClient::FreesoundClient(juce::String secret)
{
	clientSecret = secret;
	header = "Token " + clientSecret;
	auth = Token;
}

FreesoundClient::FreesoundClient(juce::String id, juce::String secret)
{
	clientID = id;
	clientSecret = secret;
	auth = OAuth;
}

//Function for doing the authorization, the mode selects between LOGOUT_AUTHORIZE(0) and AUTHORIZE(1)
void FreesoundClient::authenticationOnBrowser(int mode, Callback cb)
{
    juce::URL url;
	if(mode == 0){url = URIS::uri(URIS::LOGOUT_AUTHORIZE, juce::StringArray());}
	else{url = URIS::uri(URIS::AUTHORIZE, juce::StringArray()); }
	url = url.withParameter("client_id", clientID);
	url = url.withParameter("response_type", "code");
	url.launchInDefaultBrowser();
	cb();
}

void FreesoundClient::exchangeToken(juce::String authCode, Callback cb)
{
    juce::StringPairArray params;
	params.set("client_id", clientID);
	params.set("client_secret", clientSecret);
	params.set("grant_type", "authorization_code");
	params.set("code", authCode);

    juce::URL url = URIS::uri(URIS::ACCESS_TOKEN, juce::StringArray());
	FSRequest request(url, *this);
	Response resp = request.request(params);
	int resultCode = resp.first;
	if (resultCode == 200) {
        juce::var response = resp.second;
		accessToken = response["access_token"];
		refreshToken = response["refresh_token"];
		header = "Bearer " + accessToken;
	}
	cb();
}

void FreesoundClient::refreshAccessToken(Callback cb) {
    juce::StringPairArray params;
	params.set("client_id", clientID);
	params.set("client_secret", clientSecret);
	params.set("grant_type", "refresh_token");
	params.set("refresh_token", refreshToken);

    juce::URL url = URIS::uri(URIS::ACCESS_TOKEN, juce::StringArray());
	FSRequest request(url, *this);
	Response resp = request.request(params);
	int resultCode = resp.first;
	if (resultCode == 200) {
        juce::var response = resp.second;
		accessToken = response["access_token"];
		refreshToken = response["refresh_token"];
		header = "Bearer " + accessToken;
	}
	cb();
}

// https://freesound.org/docs/api/resources_apiv2.html#text-search
SoundList FreesoundClient::textSearch(juce::String query, juce::String filter, juce::String sort, int groupByPack, int page, int pageSize, juce::String fields, juce::String descriptors, int normalized)
{
    juce::StringPairArray params;
	params.set("query", query);
	params.set("sort", sort);
	params.set("group_by_pack", juce::String(groupByPack));

	if (filter.isNotEmpty()) {
		params.set("filter", filter);
	}

	if (page != -1) {
		params.set("page", juce::String(page));
	}

	if (pageSize != -1) {
		params.set("page_size", juce::String(pageSize));
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

    juce::URL url = URIS::uri(URIS::TEXT_SEARCH, juce::StringArray());
	FSRequest request(url, *this);
	Response resp = request.request(params,juce::String(),false);
	int resultCode = resp.first;
	if (resultCode == 200) {
        juce::var response = resp.second;
		SoundList returnedSounds(response);
		return returnedSounds;
	}
	return SoundList();
}

SoundList FreesoundClient::contentSearch(juce::String target, juce::String descriptorsFilter, int page, int pageSize, juce::String fields, juce::String descriptors, int normalized)
{
    juce::StringPairArray params;

	params.set("target", target);

	if (descriptorsFilter.isNotEmpty()) {
		params.set("descriptors_filter", descriptorsFilter);
	}

	if (page != -1) {
		params.set("page", juce::String(page));
	}

	if (pageSize != -1) {
		params.set("page_size", juce::String(pageSize));
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

    juce::URL url = URIS::uri(URIS::CONTENT_SEARCH, juce::StringArray());
	FSRequest request(url, *this);
	Response resp = request.request(params, juce::String(), false);
	int resultCode = resp.first;
	if (resultCode == 200) {
        juce::var response = resp.second;
		SoundList returnedSounds(response);
		return returnedSounds;
	}
	return SoundList();
}

FSList FreesoundClient::fetchNextPage(FSList soundList)
{
	FSRequest request(soundList.getNextPage(), *this);
	Response resp = request.request(juce::StringPairArray(), juce::String(), false);
	int resultCode = resp.first;
	if (resultCode == 200) {
        juce::var response = resp.second;
		FSList returnedSounds(response);
		return returnedSounds;
	}
	return FSList();
}

FSList FreesoundClient::fetchPreviousPage(FSList soundList)
{
	FSRequest request(soundList.getPreviousPage(), *this);
	Response resp = request.request(juce::StringPairArray(), juce::String(), false);
	int resultCode = resp.first;
	if (resultCode == 200) {
        juce::var response = resp.second;
		FSList returnedSounds(response);
		return returnedSounds;
	}
	return FSList();
}

SoundList FreesoundClient::fetchNextPage(SoundList soundList)
{
	FSRequest request(soundList.getNextPage(), *this);
	Response resp = request.request(juce::StringPairArray(), juce::String(), false);
	int resultCode = resp.first;
	if (resultCode == 200) {
        juce::var response = resp.second;
		SoundList returnedSounds(response);
		return returnedSounds;
	}
	return SoundList();
}

SoundList FreesoundClient::fetchPreviousPage(SoundList soundList)
{
	FSRequest request(soundList.getPreviousPage(), *this);
	Response resp = request.request(juce::StringPairArray(), juce::String(), false);
	int resultCode = resp.first;
	if (resultCode == 200) {
        juce::var response = resp.second;
		SoundList returnedSounds(response);
		return returnedSounds;
	}
	return SoundList();
}

FSSound FreesoundClient::getSound(juce::String id, juce::String fields)
{
    juce::URL url = URIS::uri(URIS::SOUND, juce::StringArray(id));
    juce::StringPairArray params;
	if (fields.isNotEmpty()) {
		params.set("fields", fields);
	}
	
	FSRequest request(url, *this);
	Response resp = request.request(params, juce::String(), false);
	int resultCode = resp.first;
	if (resultCode == 200) {
        juce::var response = resp.second;
		FSSound returnedSound(response);
		return returnedSound;
	}

	return FSSound();
}

juce::var FreesoundClient::getSoundAnalysis(juce::String id, juce::String descriptors, int normalized)
{
    juce::StringPairArray params;

	if (descriptors.isNotEmpty()) {
		params.set("descriptors", descriptors);
	}

	if (normalized != 0) {
		params.set("normalized", "1");
	}

    juce::URL url = URIS::uri(URIS::SOUND_ANALYSIS, id);
	FSRequest request(url, *this);
	Response resp = request.request(params, juce::String(), false);
	int resultCode = resp.first;
	if (resultCode == 200) {
        juce::var response = resp.second;
		return response;
	}
	return juce::var();
}

SoundList FreesoundClient::getSimilarSounds(juce::String id, juce::String descriptorsFilter, int page, int pageSize, juce::String fields, juce::String descriptors, int normalized)
{
    juce::StringPairArray params;

	if (descriptorsFilter.isNotEmpty()) {
		params.set("descriptors_filter", descriptorsFilter);
	}

	if (page != -1) {
		params.set("page", juce::String(page));
	}

	if (pageSize != -1) {
		params.set("page_size", juce::String(pageSize));
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

    juce::URL url = URIS::uri(URIS::SIMILAR_SOUNDS, id);
	FSRequest request(url, *this);
	Response resp = request.request(params, juce::String(), false);
	int resultCode = resp.first;
	if (resultCode == 200) {
        juce::var response = resp.second;
		SoundList returnedSounds(response);
		return returnedSounds;
	}
	return SoundList();
}

std::unique_ptr<juce::URL::DownloadTask> FreesoundClient::downloadSound(FSSound sound, const juce::File & location, juce::URL::DownloadTask::Listener * listener)
{
    juce::URL address = sound.getDownload();
	return address.downloadToFile(location, "Authorization: " + header, listener);
}

std::unique_ptr<juce::URL::DownloadTask> FreesoundClient::downloadOGGSoundPreview(FSSound sound, const juce::File & location, juce::URL::DownloadTask::Listener * listener)
{
    juce::URL address = sound.getOGGPreviewURL();
    return address.downloadToFile(location, "", listener);
}

std::unique_ptr<juce::URL::DownloadTask> FreesoundClient::downloadMP3SoundPreview(FSSound sound, const juce::File & location, juce::URL::DownloadTask::Listener * listener)
{
	juce::URL address = sound.getMP3PreviewURL();
	return address.downloadToFile(location, "", listener);
}

int FreesoundClient::uploadSound(const juce::File & fileToUpload, juce::String tags, juce::String description, juce::String name, juce::String license, juce::String pack, juce::String geotag, Callback cb)
{
    juce::StringPairArray params;

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

    juce::URL url = URIS::uri(URIS::UPLOAD);
	url = url.withFileToUpload("audiofile", fileToUpload, "audio/*");
	FSRequest request(url, *this);
	Response resp = request.request(params, juce::String(), false);
	int resultCode = resp.first;
	if (resultCode == 200) {
        juce::var response = resp.second;
		cb();
		return response["id"];
	}
	cb();
	return 0;
}

int FreesoundClient::describeSound(juce::String uploadFilename, juce::String description, juce::String license, juce::String name, juce::String tags, juce::String pack, juce::String geotag)
{
    juce::StringPairArray params;
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

    juce::URL url = URIS::uri(URIS::DESCRIBE);
	FSRequest request(url, *this);
	Response resp = request.request(params, juce::String(), false);
	int resultCode = resp.first;
	if (resultCode == 200) {
	}

	return resp.second["id"];
}

juce::var FreesoundClient::pendingUploads()
{
    juce::URL url = URIS::uri(URIS::PENDING);
	FSRequest request(url, *this);
	Response resp = request.request(juce::StringPairArray(), juce::String(), false);
	int resultCode = resp.first;
	if (resultCode == 200) {
	}

	return resp.second;
}

void FreesoundClient::editSoundDescription(juce::String id, juce::String name, juce::String tags, juce::String description, juce::String license, juce::String pack, juce::String geotag, Callback cb)
{
    juce::StringPairArray params;

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

    juce::URL url = URIS::uri(URIS::EDIT, id);
	FSRequest request(url, *this);
	Response resp = request.request(params, juce::String(), false);
	int resultCode = resp.first;
	if (resultCode == 200) {
	}

	cb();

}

void FreesoundClient::bookmarkSound(juce::String id, juce::String name, juce::String category, Callback cb)
{
    juce::StringPairArray params;

	if (name.isNotEmpty()) {
		params.set("name", name);
	}

	if (category.isNotEmpty()) {
		params.set("category", category);
	}

    juce::URL url = URIS::uri(URIS::BOOKMARK, id);
	FSRequest request(url, *this);
	Response resp = request.request(params, juce::String(), false);
	int resultCode = resp.first;
	if (resultCode == 200) {
	}
	cb();
}

void FreesoundClient::rateSound(juce::String id, int rating, Callback cb)
{
    juce::StringPairArray params;
	params.set("rating", juce::String(rating));

    juce::URL url = URIS::uri(URIS::RATE, id);
	FSRequest request(url, *this);
	Response resp = request.request(params, juce::String(), false);
	int resultCode = resp.first;
	if (resultCode == 200) {
	}

	cb();
}

void FreesoundClient::commentSound(juce::String id, juce::String comment, Callback cb)
{
    juce::StringPairArray params;
	params.set("comment", comment);

    juce::URL url = URIS::uri(URIS::RATE, id);
	FSRequest request(url, *this);
	Response resp = request.request(params, juce::String(), false);
	int resultCode = resp.first;
	if (resultCode == 200) {
	}
	cb();
}

FSUser FreesoundClient::getUser(juce::String user)
{
    juce::URL url = URIS::uri(URIS::USER, juce::StringArray(user));
	FSRequest request(url, *this);
	Response resp = request.request(juce::StringPairArray(), juce::String(), false);
	int resultCode = resp.first;
	if (resultCode == 200) {
        juce::var response = resp.second;
		FSUser returnedUser(response);
		return returnedUser;
	}

	return FSUser();
}

SoundList FreesoundClient::getUserSounds(juce::String username, juce::String descriptorsFilter, int page, int pageSize, juce::String fields, juce::String descriptors, int normalized)
{
    juce::StringPairArray params;

	if (descriptorsFilter.isNotEmpty()) {
		params.set("descriptors_filter", descriptorsFilter);
	}

	if (page != -1) {
		params.set("page", juce::String(page));
	}

	if (pageSize != -1) {
		params.set("page_size", juce::String(pageSize));
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

    juce::URL url = URIS::uri(URIS::USER_SOUNDS, username);
	FSRequest request(url, *this);
	Response resp = request.request(params, juce::String(), false);
	int resultCode = resp.first;
	if (resultCode == 200) {
        juce::var response = resp.second;
		SoundList returnedSounds(response);
		return returnedSounds;
	}
	return SoundList();
}

FSList FreesoundClient::getUserBookmarkCategories(juce::String username)
{
    juce::URL url = URIS::uri(URIS::USER_BOOKMARK_CATEGORIES, username);
	FSRequest request(url, *this);
	Response resp = request.request(juce::StringPairArray(), juce::String(), false);
	int resultCode = resp.first;
	if (resultCode == 200) {
        juce::var response = resp.second;
		FSList returnedSounds(response);
		return returnedSounds;
	}
	return FSList();
}

FSList FreesoundClient::getUserBookmarkCategoriesSounds(juce::String username, juce::String bookmarkCategory)
{	
    juce::StringArray params;
	params.add(username);
	params.add(bookmarkCategory);
    juce::URL url = URIS::uri(URIS::USER_BOOKMARK_CATEGORY_SOUNDS, params);
	FSRequest request(url, *this);
	Response resp = request.request(juce::StringPairArray(), juce::String(), false);
	int resultCode = resp.first;
	if (resultCode == 200) {
        juce::var response = resp.second;
		FSList returnedSounds(response);
		return returnedSounds;
	}
	return FSList();
}

FSList FreesoundClient::getUserPacks(juce::String username)
{
    juce::URL url = URIS::uri(URIS::USER_PACKS, username);
	FSRequest request(url, *this);
	Response resp = request.request(juce::StringPairArray(), juce::String(), false);
	int resultCode = resp.first;
	if (resultCode == 200) {
        juce::var response = resp.second;
		FSList returnedSounds(response);
		return returnedSounds;
	}
	return FSList();
}

FSPack FreesoundClient::getPack(juce::String id)
{
    juce::URL url = URIS::uri(URIS::PACK, juce::StringArray(id));
	FSRequest request(url, *this);
	Response resp = request.request(juce::StringPairArray(), juce::String(), false);
	int resultCode = resp.first;
	if (resultCode == 200) {
        juce::var response = resp.second;
		FSPack returnedUser(response);
		return returnedUser;
	}

	return FSPack();
}

SoundList FreesoundClient::getPackSounds(juce::String id, juce::String descriptorsFilter, int page, int pageSize, juce::String fields, juce::String descriptors, int normalized)
{
    juce::StringPairArray params;

	if (descriptorsFilter.isNotEmpty()) {
		params.set("descriptors_filter", descriptorsFilter);
	}

	if (page != -1) {
		params.set("page", juce::String(page));
	}

	if (pageSize != -1) {
		params.set("page_size", juce::String(pageSize));
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

    juce::URL url = URIS::uri(URIS::PACK_SOUNDS, juce::StringArray(id));
	FSRequest request(url, *this);
	Response resp = request.request(params, juce::String(), false);
	int resultCode = resp.first;
	if (resultCode == 200) {
        juce::var response = resp.second;
		SoundList returnedSounds(response);
		return returnedSounds;
	}
	return SoundList();
}

std::unique_ptr<juce::URL::DownloadTask> FreesoundClient::downloadPack(FSPack pack, const juce::File & location, juce::URL::DownloadTask::Listener * listener)
{

    juce::URL address = URIS::uri(URIS::PACK_DOWNLOAD, juce::StringArray(pack.getID()));
	return address.downloadToFile(location, this->header, listener);
	
}

FSUser FreesoundClient::getMe()
{
    juce::URL url = URIS::uri(URIS::ME, juce::StringArray());
	FSRequest request(url, *this);
	Response resp = request.request(juce::StringPairArray(), juce::String(), false);
	int resultCode = resp.first;
	if (resultCode == 200) {
        juce::var response = resp.second;
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

juce::String FreesoundClient::getToken()
{
	return token;
}

juce::String FreesoundClient::getHeader()
{
	return header;
}

juce::String FreesoundClient::getClientID()
{
	return clientID;
}

Response FSRequest::request(juce::StringPairArray params, juce::String data, bool postLikeRequest)
{

    juce::URL url = uri;
    juce::String header;
	int statusCode = -1;
    juce::StringPairArray responseHeaders;
	if (data.isNotEmpty()) { url = url.withPOSTData(data); }
	if (params.size() != 0) { url = url.withParameters(params); }
	if (client.isTokenNotEmpty()) { header = "Authorization: " + client.getHeader(); }


	//Try to open a stream with this information.
	if (auto stream = std::unique_ptr<juce::InputStream>(url.createInputStream(postLikeRequest, nullptr, nullptr, header,
		FREESOUND_API_REQUEST_TIMEOUT, // timeout in millisecs
		&responseHeaders, &statusCode)))
	{
		//Stream created successfully, store the response, log it and return the response in a pair containing (statusCode, response)
        juce::String resp = stream->readEntireStreamAsString();
        juce::var response = juce::JSON::parse(resp);
		return Response(statusCode, response);
	}
	//Couldnt create stream, return (-1, emptyVar)
	return Response(statusCode, juce::var());
}

FSList::FSList()
{
	count = 0;
}

FSList::FSList(juce::var response)
{
	count = response["count"]; //getIntValue
	nextPage = response["next"];
	previousPage = response["previous"];
	results = response["results"];
}

juce::String FSList::getNextPage()
{
	return nextPage;
}

juce::String FSList::getPreviousPage()
{
	return previousPage;
}

juce::var FSList::getResults()
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

FSSound::FSSound(juce::var sound)
{
	id = sound["id"];
	url = juce::URL(sound["url"]);
	name = sound["name"];
	tags = juce::StringArray();
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
	pack = juce::URL(sound["pack"]);
	download = juce::URL(sound["download"]);
	bookmark = juce::URL(sound["bookmark"]);
	previews = sound["previews"];
    images = sound["images"];
	numDownloads = sound["images"];
	avgRating = sound["num_downloads"];
	numRatings = sound["avg_rating"];
	rate = juce::URL(sound["rate"]);
	comments = juce::URL(sound["comments"]);
	numComments = sound["num_comments"];
	comment = juce::URL(sound["comment"]);
	similarSounds = juce::URL(sound["similar_sounds"]);
	analysis = sound["analysis"];
	analysisStats = juce::URL(sound["analysis_stats"]);
	analysisFrames = juce::URL(sound["analysis_frames"]);
	acAnalysis = sound["ac_analysis"];

}

juce::URL FSSound::getDownload()
{
	return download;
}

juce::URL FSSound::getOGGPreviewURL()
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
    return juce::URL(previews["preview-hq-ogg"]);
}

juce::URL FSSound::getMP3PreviewURL()
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
	return juce::URL(previews["preview-hq-mp3"]);
}

FSUser::FSUser()
{
}

FSUser::FSUser(juce::var user)
{
	profile = juce::URL(user["url"]);
	about = user["about"];
	username = user["username"];
	homepage = juce::URL(user["homepage"]);
	avatar = user["avatar"];
	dateJoined = user["date_joined"];
	numSounds = user["num_sounds"];
	sounds = juce::URL(user["sounds"]);
	numPacks = user["num_packs"];
	packs = juce::URL(user["packs"]);
	numPosts = user["num_posts"];
	numComments = user["num_comments"];
	bookmarks = juce::URL(user["bookmarks"]);

}

FSPack::FSPack()
{
}

FSPack::FSPack(juce::var pack)
{
	id = pack["id"];
	url = juce::URL(pack["url"]);
	description = pack["description"];
	created = pack["created"];
	name = pack["name"];
	username = pack["username"];
	numSounds = pack["num_sounds"];
	sounds = juce::URL(pack["sounds"]);
	numDownloads = pack["num_downloads"];
}

juce::String FSPack::getID()
{
	return id;
}

juce::Array<FSSound> SoundList::toArrayOfSounds()
{


	juce::Array<FSSound> arrayOfSounds;

	for (int i = 0; i < results.size(); i++)
	{
		arrayOfSounds.add(FSSound(results[i]));
	}

	return arrayOfSounds;
}
