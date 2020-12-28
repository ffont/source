/*
  ==============================================================================

	A JUCE client for the Freesound API.
	
	Find the API documentation at http://www.freesound.org/docs/api/.
	
	Apply for an API key at http://www.freesound.org/api/apply/.
	
	The client automatically maps function arguments to http parameters of the API.
	JSON results are converted to JUCE objects. The main object types (Sound,
	User, Pack) are augmented with the corresponding API calls.


    Created: 30 May 2019 12:49:28pm
    Author:  Antonio Ramires (Music Technology Group - Universitat Pompeu Fabra)

  ==============================================================================
*/


#pragma once

#include <JuceHeader.h>

/**
 * \typedef	std::pair<int, var> Response
 *
 * \brief	Defines an alias representing a response for a request 
 */

typedef std::pair<int, var> Response;

/**
 * \typedef	std::function<void()> Callback
 *
 * \brief	Defines an alias representing a void callback function, to be called when
 *			the calls to the API finish
 */

typedef std::function<void()> Callback;

/**
 * \class	FSList
 *
 * \brief	Class representing a page from a Freesound response. Contains methods for 
 *			automatically paging through the different pages, knowing the number of 
 *			total results and the results on the page.
 *
 * \author	Antonio
 * \date	09/07/2019
 */

class FSList {
protected:
	/** \brief	Total number of results returned from the FS response */
	int count;
	/** \brief	URL for accessing the next page of results */
	String nextPage;
	/** \brief	URL for accessing the previous page of results */
	String previousPage;
	/** \brief	var variable containing the results of the page */
	var results;
public:

	/**
	 * \fn	FSList::FSList();
	 *
	 * \brief	Default constructor, creates an empty object
	 *
	 * \author	Antonio
	 * \date	09/07/2019
	 */

	FSList();

	/**
	 * \fn	FSList::FSList(var response);
	 *
	 * \brief	Constructor from the var object which contains the response from FS API,
	 *			automatically obtaining the next and previous page; the total of results
	 *			and the results of the page
	 *
	 * \author	Antonio
	 * \date	09/07/2019
	 *
	 * \param	response	The response from a FS API call which returns a list.
	 */

	FSList(var response);

	/**
	 * \fn	String FSList::getNextPage();
	 *
	 * \brief	Returns the URL for the next page
	 *
	 * \author	Antonio
	 * \date	09/07/2019
	 *
	 * \returns	The next page URL.
	 */

	String getNextPage();

	/**
	 * \fn	String FSList::getPreviousPage();
	 *
	 * \brief	Returns the URL for the previous page
	 *
	 * \author	Antonio
	 * \date	09/07/2019
	 *
	 * \returns	The previous page URL.
	 */

	String getPreviousPage();

	/**
	 * \fn	var FSList::getResults();
	 *
	 * \brief	Returns the results from the current page
	 *
	 * \author	Antonio
	 * \date	09/07/2019
	 *
	 * \returns	The results of the current page.
	 */

	var getResults();

	/**
	 * \fn	int FSList::getCount();
	 *
	 * \brief	Gets the number of entries in the complete list
	 *
	 * \author	Antonio
	 * \date	09/07/2019
	 *
	 * \returns	The count of entries in the list.
	 */

	int getCount();

};

/**
 * \class	URIS
 *
 * \brief	A class which cointains all variables and methods for creating URL objects
 *			for performing the queries to Freesound
 *
 * \author	Antonio
 * \date	09/07/2019
 */

class URIS {

public:

	/** \brief	Freesound website */
	static String HOST;
	/** \brief	The base URL for the requests*/
	static String BASE;
	/** \brief	The text search resource*/
	static String TEXT_SEARCH;
	/** \brief	The content search resource*/
	static String CONTENT_SEARCH;
	/** \brief	The combined search resource, not implemented*/
	static String COMBINED_SEARCH;
	/** \brief	The sound instance resource*/
	static String SOUND;
	/** \brief	The sound analysis resource */
	static String SOUND_ANALYSIS;
	/** \brief	The similar sounds resource */
	static String SIMILAR_SOUNDS;
	/** \brief	The sound comments resource*/
	static String COMMENTS;
	/** \brief	The download sound resource*/
	static String DOWNLOAD;
	/** \brief	The upload sound resource*/
	static String UPLOAD;
	/** \brief	The describe sound resource*/
	static String DESCRIBE;
	/** \brief	The pending uploads resource*/
	static String PENDING;
	/** \brief	The bookmark sound resource*/
	static String BOOKMARK;
	/** \brief	The rate sound resource*/
	static String RATE;
	/** \brief	The comment sound resource*/
	static String COMMENT;
	/** \brief	The authorization resource */
	static String AUTHORIZE;
	/** \brief	The logout resource*/
	static String LOGOUT;
	/** \brief	The logout and authorize resource*/
	static String LOGOUT_AUTHORIZE;
	/** \brief	The get access token resource*/
	static String ACCESS_TOKEN;
	/** \brief	The me resource*/
	static String ME;
	/** \brief	The user instance resource*/
	static String USER;
	/** \brief	The get user sounds resource*/
	static String USER_SOUNDS;
	/** \brief	The get user packs resource*/
	static String USER_PACKS;
	/** \brief	The get user bookmark categories resources*/
	static String USER_BOOKMARK_CATEGORIES;
	/** \brief	The user bookmark category sounds resources*/
	static String USER_BOOKMARK_CATEGORY_SOUNDS;
	/** \brief	The get pack instance resource*/
	static String PACK;
	/** \brief	The pack sounds */
	static String PACK_SOUNDS;
	/** \brief	The pack download resource*/
	static String PACK_DOWNLOAD;
	/** \brief	The confirmation URL*/
	static String CONFIRMATION;
	/** \brief	The edit sound resource*/
	static String EDIT;

	/**
	 * \fn	URIS::URIS()
	 *
	 * \brief	Default constructor, creates an empty object
	 *
	 * \author	Antonio
	 * \date	09/07/2019
	 */

	URIS(){
	}

	/**
	 * \fn	URIS::~URIS()
	 *
	 * \brief	Destructor
	 *
	 * \author	Antonio
	 * \date	09/07/2019
	 */

	~URIS() {
	}

	/**
	 * \fn	static URL URIS::uri(String uri, StringArray replacements = StringArray());
	 *
	 * \brief	Creates an URL given a URI from the static URIS and a String array with
	 *			the parameters
	 *
	 * \author	Antonio
	 * \date	09/07/2019
	 *
	 * \param	uri				URI of the resource.
	 * \param	replacements	(Optional) The arguments for the URL.
	 *
	 * \returns	The URL for the request.
	 */

	static URL uri(String uri, StringArray replacements = StringArray());
};

/**
 * \class	FSUser
 *
 * \brief	A class which represents a Freesound user and its information.
 *
 * \author	Antonio
 * \date	09/07/2019
 */

class FSUser {

public:
	/** \brief	The URL of the user profile in Freesound*/
	URL profile;
	/** \brief	The username */
	String username;
	/** \brief	The ‘about’ text of users’ profile (if indicated)*/
	String about;
	/** \brief	The URI of users’ homepage outside Freesound (if indicated)*/
	URL homepage;
	/** \brief	Dictionary including the URIs for the avatar of the user. 
				The avatar is presented in three sizes Small, Medium and Large, which 
				correspond to the three fields in the dictionary. 
				If user has no avatar, this field is null.*/
	var avatar;
	/** \brief	The date when the user joined Freesound */
	String dateJoined;
	/** \brief	Number of sounds uploaded by the user*/
	int numSounds;
	/** \brief	The URI for a list of sounds by the user*/
	URL sounds;
	/** \brief	The number of packs by the user*/
	int numPacks;
	/** \brief	The URI for a list of packs by the user*/
	URL packs;
	/** \brief	The number of forum posts by the user */
	int numPosts;
	/** \brief	The number of comments that user made in other users’ sounds*/
	int numComments;   
	/** \brief	The URI for a list of bookmark categories by the user*/
	URL bookmarks;

	/**
	 * \fn	FSUser::FSUser();
	 *
	 * \brief	Default constructor, creates an empy User object
	 *
	 * \author	Antonio
	 * \date	09/07/2019
	 */

	FSUser();

	/**
	 * \fn	FSUser::FSUser(var user);
	 *
	 * \brief	Constructor from the response of a user instance request
	 *
	 * \author	Antonio
	 * \date	09/07/2019
	 *
	 * \param	user	The response of a user instance request.
	 */

	FSUser(var user);
};

/**
 * \class	FSPack
 *
 * \brief	A class which implements the Pack instance.
 *
 * \author	Antonio
 * \date	09/07/2019
 */

class FSPack {

public:
	/** \brief	The unique identifier of this pack*/
	String id;
	/** \brief	The URI for this pack on the Freesound website*/
	URL url;
	/** \brief	The URI for this pack on the Freesound website */
	String description;
	/** \brief	The date when the pack was created */
	String created;
	/** \brief	The name user gave to the pack */
	String name;
	/** \brief	Username of the creator of the pack */
	String username;
	/** \brief	The number of sounds in the pack */
	int numSounds;
	/** \brief	The URI for a list of sounds in the pack */
	URL sounds;
	/** \brief	The number of times this pack has been downloaded */
	int numDownloads;

	/**
	 * \fn	FSPack::FSPack();
	 *
	 * \brief	Default constructor, creates an empty Pack object
	 *
	 * \author	Antonio
	 * \date	09/07/2019
	 */

	FSPack();

	/**
	 * \fn	FSPack::FSPack(var pack);
	 *
	 * \brief	Constructor from the response of a pack instance request
	 *
	 * \author	Antonio
	 * \date	09/07/2019
	 *
	 * \param	pack	 The response of a pack instance request.
	 */

	FSPack(var pack);

	/**
	 * \fn	String FSPack::getID();
	 *
	 * \brief	Gets the identifier of the pack
	 *
	 * \author	Antonio
	 * \date	09/07/2019
	 *
	 * \returns The unique identifier of this pack.
	 */

	String getID();
};

/**
 * \class	FSSound
 *
 * \brief	A class which implements the Sound instance.
 *
 * \author	Antonio
 * \date	09/07/2019
 */

class FSSound {
public:
	/** \brief	The sound’s unique identifier */
	String id;
	/** \brief	The URI for this sound on the Freesound website*/
	URL url;
	/** \brief	The name user gave to the sound*/
	String name;
	/** \brief	An array of tags the user gave to the sound */
	StringArray tags;
	/** \brief	The description the user gave to the sound*/
	String description;
	/** \brief	Latitude and longitude of the geotag separated by spaces */
	String geotag;
	/** \brief	The date when the sound was uploaded */
	String created;
	/** \brief	The license under which the sound is available */
	String license;
	/** \brief	The type of sound (wav, aif, aiff, mp3, m4a or flac) */
	String format;
	/** \brief	The number of channels */
	int channels;
	/** \brief	The size of the file in bytes */
	int filesize;
	/** \brief	The bit rate of the sound in kbps */
	int bitrate;
	/** \brief	The bit depth of the sound */
	int bitdepth;
	/** \brief	The duration of the sound in seconds */
	int duration;
	/** \brief	The samplerate of the sound */
	int samplerate;
	/** \brief	The username of the uploader of the sound */
	String user;
	/** \brief	If the sound is part of a pack, this URI points to that pack’s API resource */
	URL pack;
	/** \brief	The URI for retrieving the original sound */
	URL download;
	/** \brief	The URI for bookmarking the sound */
	URL bookmark;
	/** \brief	Dictionary containing the URIs for mp3 and ogg versions of the sound */
	var previews;
	/** \brief	Dictionary including the URIs for spectrogram and waveform visualizations of the sound */
	var images;
	/** \brief	The number of times the sound was downloaded */
	int numDownloads;
	/** \brief	The average rating of the sound */
	float avgRating;
	/** \brief	The number of times the sound was rated */
	int numRatings;
	/** \brief	The URI for rating the sound */
	URL rate;
	/** \brief	The URI of a paginated list of the comments of the sound */
	URL comments;
	/** \brief	The number of comments */
	int numComments;
	/** \brief	The URI to comment the sound*/
	URL comment;
	/** \brief	URI pointing to the similarity resource (to get a list of similar sounds) */
	URL similarSounds;
	/** \brief	Dictionary containing requested descriptors information according to the 
				descriptors request parameter */
	var analysis;
	/** \brief	URI pointing to the complete analysis results of the sound */
	URL analysisStats;
	/** \brief	The URI for retrieving a JSON file with analysis information for each frame of the sound */
	URL analysisFrames;
	/** \brief	Dictionary containing the results of the AudioCommons analysis for the given sound */
	var acAnalysis;

	/**
	 * \fn	FSSound::FSSound();
	 *
	 * \brief	Default constructor, creates an empty FSSound object
	 *	
	 * \author	Antonio
	 * \date	09/07/2019
	 */

	FSSound();

	/**
	 * \fn	FSSound::FSSound(var sound);
	 *
	 * \brief	Constructor from the response of a sound instance request
	 *
	 * \author	Antonio
	 * \date	09/07/2019
	 *
	 * \param	sound	The response of a sound instance request
	 */

	FSSound(var sound);

	/**
	 * \fn	URL FSSound::getDownload();
	 *
	 * \brief	Gets the sound download URL
	 *
	 * \author	Antonio
	 * \date	09/07/2019
	 *
	 * \returns	The sound download URL.
	 */

	URL getDownload();
    
    /**
     * \fn    URL FSSound::getOGGPreviewURL();
     *
     * \brief    Gets the sound the URL of the OGG preview of a sound
     *
     * \author    Frederic
     * \date    12/09/2019
     *
     * \returns    The OGG preview URL.
     */
    
    URL getOGGPreviewURL();

	URL getMP3PreviewURL();

};

/**
 * \class	SoundList
 *
 * \brief	A class which implements the sound list returned from certain queries to Freesound API,
 *			which inherits FSList, containing a method to convert itself to an array of FSSounds.
 *
 * \author	Antonio
 * \date	09/07/2019
 */

class SoundList : public FSList {
	using FSList::FSList;
public:

	/**
	 * \fn	Array<FSSound> SoundList::toArrayOfSounds();
	 *
	 * \brief	Creates a copy of this object as an array of FSSound
	 *
	 * \author	Antonio
	 * \date	09/07/2019
	 *
	 * \returns	A copy of this object as an array of FSSound;
	 */

	Array<FSSound> toArrayOfSounds();
};

/**
 * \class	FreesoundClient
 *
 * \brief	The Freesound client class which is responsible for all authorization and
 *			requests to the Freesound API
 *
 * \author	Antonio
 * \date	09/07/2019
 */

class FreesoundClient{
	
private:

	/**
	 * \enum	Authorization
	 *
	 * \brief	Values that represent the possible authorization methods
	 */

	enum Authorization
	{
		None,
		Token,
		OAuth
	};

	/** \brief	Client secret/Api key */
	String token;
	/** \brief	Client id of your API credential */
	String clientID;
	/** \brief	The client secret */
	String clientSecret;
	/** \brief	The access token */
	String accessToken;
	/** \brief	The refresh token */
	String refreshToken;
	/** \brief	The header for all the requests*/
	String header;
	/** \brief	The authentication type*/
	Authorization auth;


public:

	/**
	 * \fn	FreesoundClient::FreesoundClient();
	 *
	 * \brief	Empty construtctor
	 *
	 * \author	Antonio
	 * \date	09/07/2019
	 */

	FreesoundClient();


	/**
	 * \fn	FreesoundClient::FreesoundClient(String secret);
	 *
	 * \brief	Constructor for token based authorization
	 *
	 * \author	Antonio
	 * \date	09/07/2019
	 *
	 * \param	secret	The client secret.
	 */

	FreesoundClient(String secret);

	/**
	 * \fn	FreesoundClient::FreesoundClient(String id, String secret);
	 *
	 * \brief	Constructor for OAuth2 authorization
	 *
	 * \author	Antonio
	 * \date	09/07/2019
	 *
	 * \param	id	  	The client ID.
	 * \param	secret	The client secret.
	 */

	FreesoundClient(String id, String secret);

	/**
	 * \fn	void FreesoundClient::authenticationOnBrowser(int mode=0, Callback cb = [] {});
	 *
	 * \brief	Performs the first step of the authorization on the browser
	 *
	 * \author	Antonio
	 * \date	09/07/2019
	 *
	 * \param	mode	(Optional) The login mode 0 = logout and authorize, 1 = authorize.
	 * \param	cb  	(Optional) The callback function called in the end of the function.
	 */

	void authenticationOnBrowser(int mode=0, Callback cb = [] {});

	/**
	 * \fn	void FreesoundClient::exchangeToken(String authCode, Callback cb = [] {});
	 *
	 * \brief	Exchange token is the 3rd step of the authorization proccess, where the
	 *			authorization code is exchanged for an access token
	 *
	 * \author	Antonio
	 * \date	09/07/2019
	 *
	 * \param	authCode	The authentication code.
	 * \param	cb			(Optional) The callback function called in the end of the function.
	 */

	void exchangeToken(String authCode, Callback cb = [] {});

	/**
	 * \fn	void FreesoundClient::refreshAccessToken(Callback cb = [] {});
	 *
	 * \brief	Function for refreshing the access token, which expires in 24h
	 *
	 * \author	Antonio
	 * \date	09/07/2019
	 *
	 * \param	cb	(Optional) The callback function called in the end of the function.
	 */

	void refreshAccessToken(Callback cb = [] {});

	/**
	 * \fn	SoundList FreesoundClient::textSearch(String query, String filter=String(), String sort="score", int groupByPack=0, int page=-1, int pageSize=-1, String fields = String(), String descriptors = String(), int normalized=0);
	 *
	 * \brief	This resource allows searching sounds in Freesound by matching their tags and other kinds of metadata.
	 *
	 * \author	Antonio
	 * \date	09/07/2019
	 *
	 * \param	query	   	The text query.
	 * \param	filter	   	(Optional) Allows filtering query results.
	 * \param	sort	   	(Optional) Indicates how query results should be sorted.
	 * \param	groupByPack	(Optional) This parameter represents a boolean option to indicate whether to group packs
	 *						in a single result
	 * \param	page	   	(Optional) Query results are paginated, this parameter indicates what page should be returned.
	 * \param	pageSize   	(Optional) Indicates the number of sounds per page to include in the result.
	 * \param	fields	   	(Optional) Indicates which sound properties should be included in every sound of the response.
	 * \param	descriptors	(Optional) Indicates which sound content-based descriptors should be included in every sound of the response.
	 * \param	normalized 	(Optional) Indicates whether the returned sound content-based descriptors should be normalized or not.
	 *
	 * \returns	A SoundList with the text search results.
	 */

	SoundList textSearch(String query, String filter=String(), String sort="score", int groupByPack=0, int page=-1, int pageSize=-1, String fields = String(), String descriptors = String(), int normalized=0);

	/**
	 * \fn	SoundList FreesoundClient::contentSearch(String target, String descriptorsFilter=String(), int page = -1, int pageSize = -1, String fields = String(), String descriptors = String(), int normalized = 0);
	 *
	 * \brief	Content search
	 *
	 * \author	Antonio
	 * \date	09/07/2019
	 *
	 * \param	target			 	Target for the content search.
	 * \param	descriptorsFilter	(Optional) This parameter allows filtering query results by values of the content-based descriptors.
	 * \param	page			 	(Optional) Query results are paginated, this parameter indicates what page should be returned.
	 * \param	pageSize		 	(Optional) Indicates the number of sounds per page to include in the result.
	 * \param	fields			 	(Optional) Indicates which sound properties should be included in every sound of the response.
	 * \param	descriptors		 	(Optional) Indicates which sound content-based descriptors should be included in every sound of the response.
	 * \param	normalized		 	(Optional) Indicates whether the returned sound content-based descriptors should be normalized or not.
	 *
	 * \returns	A SoundList with the content search results.
	 */

	SoundList contentSearch(String target, String descriptorsFilter=String(), int page = -1, int pageSize = -1, String fields = String(), String descriptors = String(), int normalized = 0);

	/**
	 * \fn	FSList FreesoundClient::fetchNextPage(FSList fslist);
	 *
	 * \brief	Gets the next page of a SoundList
	 *
	 * \author	Antonio
	 * \date	09/07/2019
	 *
	 * \param	FSList	Next page of results.
	 *
	 * \returns	The next page of results.
	 */

	FSList fetchNextPage(FSList fslist);

	/**
	 * \fn	FSList FreesoundClient::fetchPreviousPage(FSList fslist);
	 *
	 * \brief	Gets the previous page of a FSList
	 * \author	Antonio
	 * \date	09/07/2019
	 *
	 * \param	FSList	Previous page of results.
	 *
	 * \returns	The previous page of results.
	 */

	FSList fetchPreviousPage(FSList fslist);
	
	/**
	 * \fn	SoundList FreesoundClient::fetchNextPage(SoundList fslist);
	 *
	 * \brief	Gets the next page of a SoundList
	 *
	 * \author	Antonio
	 * \date	09/07/2019
	 *
	 * \param	SoundList	Next page of results.
	 *
	 * \returns	The next page of results.
	 */

	SoundList fetchNextPage(SoundList fslist);

	/**
	 * \fn	SoundList FreesoundClient::fetchPreviousPage(SoundList fslist);
	 *
	 * \brief	Gets the previous page of a SoundList
	 *
	 * \author	Antonio
	 * \date	09/07/2019
	 *
	 * \param	SoundList	Previous page of results.
	 *
	 * \returns	The next page of results.
	 */

	SoundList fetchPreviousPage(SoundList fslist);

	/**
	 * \fn	FSSound FreesoundClient::getSound(String id);
	 *
	 * \brief	Gets a sound instance from its id
	 *
	 * \author	Antonio
	 * \date	09/07/2019
	 *
	 * \param	id		The sound’s unique identifier.
	 * \param	fields	(Optional) Indicates which sound properties should be included in every sound of the response.
	 *
	 * \returns	A FSSound instance.
	 */

	FSSound getSound(String id, String fields = String());

	/**
	 * \fn	var FreesoundClient::getSoundAnalysis(String id, String descriptors = String(), int normalized = 0);
	 *
	 * \brief	Retrieves of analysis information (content-based descriptors) of a sound.
	 *
	 * \author	Antonio
	 * \date	09/07/2019
	 *
	 * \param	id		   	The sound’s unique identifier.
	 * \param	descriptors	(Optional) Indicates which sound content-based descriptors should be included in every sound of the response.
	 * \param	normalized 	(Optional) Indicates whether the returned sound content-based descriptors should be normalized or not.
	 *
	 * \returns	A var dictionary containing the results of the analysis
	 */

	var getSoundAnalysis(String id, String descriptors = String(), int normalized = 0);

	/**
	 * \fn	SoundList FreesoundClient::getSimilarSounds(String id, String descriptorsFilter = String(), int page = -1, int pageSize = -1, String fields = String(), String descriptors = String(), int normalized = 0);
	 *
	 * \brief	This resource allows the retrieval of sounds similar to the given sound target
	 *
	 * \author	Antonio
	 * \date	09/07/2019
	 *
	 * \param	id				 	The id of the target sound.
	 * \param	descriptorsFilter	(Optional) This parameter allows filtering query results by values of the content-based descriptors.
	 * \param	page			 	(Optional) Query results are paginated, this parameter indicates what page should be returned.
	 * \param	pageSize		 	(Optional) Indicates the number of sounds per page to include in the result.
	 * \param	fields			 	(Optional) Indicates which sound properties should be included in every sound of the response.
	 * \param	descriptors		 	(Optional) Indicates which sound content-based descriptors should be included in every sound of the response.
	 * \param	normalized		 	(Optional) Indicates whether the returned sound content-based descriptors should be normalized or not.
	 *
	 * \returns	A SoundList with the sounds similar to a target.
	 */

	SoundList getSimilarSounds(String id, String descriptorsFilter = String(), int page = -1, int pageSize = -1, String fields = String(), String descriptors = String(), int normalized = 0);

	/**
	 * \fn	std::unique_ptr<URL::DownloadTask> FreesoundClient::downloadSound(FSSound sound, const File &location, URL::DownloadTask::Listener * listener = nullptr);
	 *
	 * \brief	This resource allows you to download a sound in its original format/quality
	 *
	 * \author	Antonio
	 * \date	09/07/2019
	 *
	 * \param 		  	sound   	The sound to be downloaded.
	 * \param 		  	location	The location for the sound to be download.
	 * \param [in,out]	listener	(Optional) If non-null, the listener for the download progress.
	 *
	 * \returns	Null if it fails, else a pointer to an URL::DownloadTask.
	 */

	std::unique_ptr<URL::DownloadTask> downloadSound(FSSound sound, const File &location, URL::DownloadTask::Listener * listener = nullptr);
    
    /**
     * \fn    std::unique_ptr<URL::DownloadTask> FreesoundClient::downloadOGGSoundPreview(FSSound sound, const File &location, URL::DownloadTask::Listener * listener = nullptr);
     *
     * \brief    This resource allows you to download the preview of a sound in OGG format. The advantage of downloading a preview instead of the original file is that the file size is lower and the format is unified.
     *
     * \author    Frederic
     * \date    12/09/2019
     *
     * \param               sound       The sound whose preview should be downloaded.
     * \param               location    The location for the sound to be download.
     * \param [in,out]    listener    (Optional) If non-null, the listener for the download progress.
     *
     * \returns    Null if it fails, else a pointer to an URL::DownloadTask.
     */
    
    std::unique_ptr<URL::DownloadTask> downloadOGGSoundPreview(FSSound sound, const File &location, URL::DownloadTask::Listener * listener = nullptr);

	std::unique_ptr<URL::DownloadTask> downloadMP3SoundPreview(FSSound sound, const File &location, URL::DownloadTask::Listener * listener = nullptr);


	/**
	 * \fn	int FreesoundClient::uploadSound(const File &fileToUpload, String tags, String description, String name = String(), String license = "Creative Commons 0", String pack = String(), String geotag = String(), Callback cb = [] {});
	 *
	 * \brief	Uploads a sound to Freesound
	 *
	 * \author	Antonio
	 * \date	09/07/2019
	 *
	 * \param	fileToUpload	The file to upload.
	 * \param	tags			The tags that will be assigned to the sound. Separate tags with spaces and join multi-words with dashes (e.g. “tag1 tag2 tag3 cool-tag4”).
	 * \param	description 	A textual description of the sound.
	 * \param	name			(Optional) The name that will be given to the sound. If not provided, filename will be used.
	 * \param	license			(Optional) The license of the sound. Must be either “Attribution”, “Attribution Noncommercial” or “Creative Commons 0”.
	 * \param	pack			(Optional) The name of the pack where the sound should be included. 
	 * \param	geotag			(Optional) Geotag information for the sound.
	 * \param	cb				(Optional) The callback function called in the end of the function.
	 *
	 * \returns	The id of the uploaded sound.
	 */

	int uploadSound(const File &fileToUpload, String tags, String description, String name = String(), String license = "Creative Commons 0", String pack = String(), String geotag = String(), Callback cb = [] {});

	/**
	 * \fn	int FreesoundClient::describeSound(String uploadFilename, String description, String license, String name = String(), String tags = String(), String pack = String(), String geotag = String() );
	 *
	 * \brief	Describe sound
	 *
	 * \author	Antonio
	 * \date	09/07/2019
	 *
	 * \param	uploadFilename	The filename of the sound to describe.
	 * \param	description   	A textual description of the sound.
	 * \param	license		  	The license of the sound. Must be either “Attribution”, “Attribution Noncommercial” or “Creative Commons 0”.
	 * \param	name		  	(Optional) The name that will be given to the sound. If not provided, filename will be used.
	 * \param	tags		  	(Optional) The tags that will be assigned to the sound. Separate tags with spaces and join multi-words with dashes (e.g. “tag1 tag2 tag3 cool-tag4”).
	 * \param	pack		  	(Optional) The name of the pack where the sound should be included.
	 * \param	geotag		  	(Optional) Geotag information for the sound.
	 *
	 * \returns	An int.
	 */

	int describeSound(String uploadFilename, String description, String license, String name = String(), String tags = String(), String pack = String(), String geotag = String() );

	/**
	 * \fn	var FreesoundClient::pendingUploads();
	 *
	 * \brief	This resource allows you to retrieve a list of audio files uploaded by the Freesound user logged in using OAuth2 that have not yet been described, processed or moderated
	 *
	 * \author	Antonio
	 * \date	09/07/2019
	 *
	 * \returns	A dictionary with a list of audio files uploaded by the Freesound user.
	 */

	var pendingUploads();

	/**
	 * \fn	void FreesoundClient::editSoundDescription(String id, String name = String(), String tags = String(), String description = String(), String license = String(), String pack = String(), String geotag = String(), Callback cb = [] {});
	 *
	 * \brief	Edit sound description
	 *
	 * \author	Antonio
	 * \date	09/07/2019
	 *
	 * \param	id		   	The identifier of the sound.
	 * \param	name	   	(Optional) The new name that will be given to the sound.
	 * \param	tags	   	(Optional) The new tags that will be assigned to the sound.
	 * \param	description	(Optional) The new textual description for the sound.
	 * \param	license	   	(Optional) The new license of the sound. Must be either “Attribution”, “Attribution Noncommercial” or “Creative Commons 0”.
	 * \param	pack	   	(Optional) The new name of the pack where the sound should be included. 
	 * \param	geotag	   	(Optional) The new geotag information for the sound.
	 * \param	cb		   	(Optional) The callback function called in the end of the function.
	 */

	void editSoundDescription(String id, String name = String(), String tags = String(), String description = String(), String license = String(), String pack = String(), String geotag = String(), Callback cb = [] {});

	/**
	 * \fn	void FreesoundClient::bookmarkSound(String id, String name = String(), String category = String(), Callback cb = [] {});
	 *
	 * \brief	This resource allows you to bookmark an existing sound.
	 *
	 * \author	Antonio
	 * \date	09/07/2019
	 *
	 * \param	id			The sound identifier.
	 * \param	name		(Optional) The new name that will be given to the bookmark
	 * \param	category	(Optional) The name of the category under the bookmark will be classified.
	 * \param	cb			(Optional) The callback function called in the end of the function.
	 */

	void bookmarkSound(String id, String name = String(), String category = String(), Callback cb = [] {});

	/**
	 * \fn	void FreesoundClient::rateSound(String id, int rating, Callback cb = [] {});
	 *
	 * \brief	This resource allows you to rate an existing sound.
	 *
	 * \author	Antonio
	 * \date	09/07/2019
	 *
	 * \param	id	  	The sound identifier.
	 * \param	rating	Integer between 0 and 5 (both included) representing the rating for the sound.
	 * \param	cb	  	(Optional) The callback function called in the end of the function.
	 */

	void rateSound(String id, int rating, Callback cb = [] {});

	/**
	 * \fn	void FreesoundClient::commentSound(String id, String comment, Callback cb = [] {});
	 *
	 * \brief	This resource allows you to post a comment to an existing sound.
	 *
	 * \author	Antonio
	 * \date	09/07/2019
	 *
	 * \param	id	   	The sound identifier.
	 * \param	comment	Comment for the sound.
	 * \param	cb	   	(Optional) The callback function called in the end of the function.
	 */

	void commentSound(String id, String comment, Callback cb = [] {});

	/**
	 * \fn	FSUser FreesoundClient::getUser(String user);
	 *
	 * \brief	This resource allows the retrieval of information about a particular Freesound user
	 *
	 * \author	Antonio
	 * \date	09/07/2019
	 *
	 * \param	user	The username of the user.
	 *
	 * \returns	A FSUser instance of the user requested.
	 */

	FSUser getUser(String user);

	/**
	 * \fn	SoundList FreesoundClient::getUserSounds(String username, String descriptorsFilter = String(), int page = -1, int pageSize = -1, String fields = String(), String descriptors = String(), int normalized = 0);
	 *
	 * \brief	This resource allows the retrieval of a list of sounds uploaded by a particular Freesound user.
	 *
	 * \author	Antonio
	 * \date	09/07/2019
	 *
	 * \param	username		 	The username of the user.
	 * \param	descriptorsFilter	(Optional) This parameter allows filtering query results by values of the content-based descriptors.
	 * \param	page			 	(Optional) Query results are paginated, this parameter indicates what page should be returned.
	 * \param	pageSize		 	(Optional) Indicates the number of sounds per page to include in the result.
	 * \param	fields			 	(Optional) Indicates which sound properties should be included in every sound of the response.
	 * \param	descriptors		 	(Optional) Indicates which sound content-based descriptors should be included in every sound of the response.
	 * \param	normalized		 	(Optional) Indicates whether the returned sound content-based descriptors should be normalized or not.
	 *
	 * \returns	A SoundList with the user sounds.
	 */

	SoundList getUserSounds(String username, String descriptorsFilter = String(), int page = -1, int pageSize = -1, String fields = String(), String descriptors = String(), int normalized = 0);

	/**
	 * \fn	FSList FreesoundClient::getUserBookmarkCategories(String username);
	 *
	 * \brief	This resource allows the retrieval of a list of bookmark categories created by a particular Freesound user.
	 *
	 * \author	Antonio
	 * \date	09/07/2019
	 *
	 * \param	username	The username of the user.
	 *
	 * \returns	A FSList with the user bookmark categories.
	 */

	FSList getUserBookmarkCategories(String username);

	/**
	 * \fn	FSList FreesoundClient::getUserBookmarkCategoriesSounds(String username, String bookmarkCategory);
	 *
	 * \brief	This resource allows the retrieval of a list of sounds from a bookmark category created by a particular Freesound user.
	 *
	 * \author	Antonio
	 * \date	09/07/2019
	 *
	 * \param	username			The username of the user.
	 * \param	bookmarkCategory	The desired bookmark category.
	 *
	 * \returns	A FSList with the user bookmark categories sounds.
	 */

	FSList getUserBookmarkCategoriesSounds(String username, String bookmarkCategory);

	/**
	 * \fn	FSList FreesoundClient::getUserPacks(String username);
	 *
	 * \brief	This resource allows the retrieval of a list of packs created by a particular Freesound user.
	 *
	 * \author	Antonio
	 * \date	09/07/2019
	 *
	 * \param	username	The username of the user.
	 *
	 * \returns	A FSList with the user packs.
	 */

	FSList getUserPacks(String username);

	/**
	 * \fn	FSPack FreesoundClient::getPack(String id);
	 *
	 * \brief	This resource allows the retrieval of a list of packs created by a particular Freesound user.
	 *
	 * \author	Antonio
	 * \date	09/07/2019
	 *
	 * \param	id	The username of the user.
	 *
	 * \returns	A FSList with the user packs.
	 */

	FSPack getPack(String id);

	/**
	 * \fn	SoundList FreesoundClient::getPackSounds(String id, String descriptorsFilter = String(), int page = -1, int pageSize = -1, String fields = String(), String descriptors = String(), int normalized = 0);
	 *
	 * \brief	This resource allows the retrieval of information about a pack.
	 *
	 * \author	Antonio
	 * \date	09/07/2019
	 *
	 * \param	id				 	The identifier of the pack.
	 * \param	descriptorsFilter	(Optional) This parameter allows filtering query results by values of the content-based descriptors.
	 * \param	page			 	(Optional) Query results are paginated, this parameter indicates what page should be returned.
	 * \param	pageSize		 	(Optional) Indicates the number of sounds per page to include in the result.
	 * \param	fields			 	(Optional) Indicates which sound properties should be included in every sound of the response.
	 * \param	descriptors		 	(Optional) Indicates which sound content-based descriptors should be included in every sound of the response.
	 * \param	normalized		 	(Optional) Indicates whether the returned sound content-based descriptors should be normalized or not.
	 *
	 * \returns	A FSPack instance of the desired pack
	 */

	SoundList getPackSounds(String id, String descriptorsFilter = String(), int page = -1, int pageSize = -1, String fields = String(), String descriptors = String(), int normalized = 0);

	/**
	 * \fn	std::unique_ptr<URL::DownloadTask> FreesoundClient::downloadPack(FSPack pack, const File &location, URL::DownloadTask::Listener * listener = nullptr);
	 *
	 * \brief	This resource allows you to download all the sounds of a pack in a single zip file.
	 *
	 * \author	Antonio
	 * \date	09/07/2019
	 *
	 * \param 		  	pack		The desored pack.
	 * \param 		  	location	The location fpr the download.
	 * \param [in,out]	listener	(Optional) If non-null, the download task listener.
	 *
	 * \returns	Null if it fails, else a pointer to the URL::DownloadTask.
	 */

	std::unique_ptr<URL::DownloadTask> downloadPack(FSPack pack, const File &location, URL::DownloadTask::Listener * listener = nullptr);

	/**
	 * \fn	FSUser FreesoundClient::getMe();
	 *
	 * \brief	Returnes a FSUser instance of the authenticated user
	 *
	 * \author	Antonio
	 * \date	09/07/2019
	 *
	 * \returns	FSUser instance of the authenticated user.
	 */

	FSUser getMe();

	/**
	 * \fn	bool FreesoundClient::isTokenNotEmpty();
	 *
	 * \brief	Queries if the token is not empty
	 *
	 * \author	Antonio
	 * \date	09/07/2019
	 *
	 * \returns	False if the token is empty, True if not.
	 */

	bool isTokenNotEmpty();

	/**
	 * \fn	String FreesoundClient::getToken();
	 *
	 * \brief	Gets the authentication token
	 *
	 * \author	Antonio
	 * \date	09/07/2019
	 *
	 * \returns	The authentication token.
	 */

	String getToken();

	/**
	 * \fn	String FreesoundClient::getHeader();
	 *
	 * \brief	Gets the authentication header
	 *
	 * \author	Antonio
	 * \date	09/07/2019
	 *
	 * \returns	The authentication header.
	 */

	String getHeader();

	/**
	 * \fn	String FreesoundClient::getClientID();
	 *
	 * \brief	Gets client identifier
	 *
	 * \author	Antonio
	 * \date	09/07/2019
	 *
	 * \returns	The client identifier.
	 */

	String getClientID();


};


class FSRequest {
public:

	/**
	 * \fn	FSRequest::FSRequest(URL uriToRequest, FreesoundClient clientToUse)
	 *
	 * \brief	The constructor for a request
	 *
	 * \author	Antonio
	 * \date	09/07/2019
	 *
	 * \param	uriToRequest	The URL for the request.
	 * \param	clientToUse 	The FSClient to use.
	 */

	FSRequest(URL uriToRequest, FreesoundClient clientToUse)
		:client(clientToUse),
		uri(uriToRequest)
	{}

	/**
	 * \fn	Response FSRequest::request(StringPairArray params = StringPairArray(), String data = String(), bool postLikeRequest = true);
	 *
	 * \brief	Make a request to FreesoundAPI
	 *
	 * \author	Antonio
	 * \date	09/07/2019
	 *
	 * \param	params		   	(Optional) The parameters for the request.
	 * \param	data		   	(Optional) The data if any.
	 * \param	postLikeRequest	(Optional) If a POST like request is desired.
	 *
	 * \returns The response for the request.
	 */

	Response request(StringPairArray params = StringPairArray(), String data = String(), bool postLikeRequest = true);

private:
	/** \brief	The URI of the rrequest */
	URL uri;
	/** \brief	The client used*/
	FreesoundClient& client;
};

