# Token
[Token](http://www.tokn.ca) is an alpha-quality search engine for logos. It’s written in Node.js with Express, Angular, and MongoDB. The performance critical parts are a Node Addon, written in C++. This repository is an AWS Elastic Beanstalk application. 

At this early stage, the results that Token returns are not as good as they could be. The main algorithm has only been given a cursory tuning.

## Dependencies
Aside from `npm` installable dependencies, Token requires three libraries:

- [GraphicsMagick](http://www.graphicsmagick.org/): A popular image manipulation utility
- [OpenCV 3.0 Beta](https://github.com/Itseez/opencv)
- [Mongo Cxx driver](https://github.com/mongodb/mongo-cxx-driver/wiki/Download-and-Compile-the-Legacy-Driver)

You will probably need to install the later two from source.

A C++ compiler and Boost are also necessary in order to build Token.

## Building
When dependencies are installed, `npm install` will build Token and install its Node dependencies. `npm install --debug` will create a debug build that prints out verbose information when logos are compared to one another.

A small program, `/build/BUILD_TYPE/test`, is created upon building that can be used to see how features are extracted from logos, and how they are compared. It must be run in the top-level directory.

## Running
`npm start` will start a Token server running. The following environment variables can be customized to change the behaviour of the server:

- `PORT`: Port that Token will listen on (default: `3000`)
- `TOKEN_DB_SERVER`: The address of the MongoDB server (`localhost`)
- `TOKEN_DB`: The name of the MongoDB database (`token`)
- `TOKEN_PASSWORD`: The admin password for Token, used for API calls

## AWS
With the [`eb` command line tool](http://docs.aws.amazon.com/elasticbeanstalk/latest/dg/eb-cli3-getting-set-up.html) you can deploy Token to your AWS account as an Elastic Beanstalk instance with the [standard](http://docs.aws.amazon.com/elasticbeanstalk/latest/dg/create_deploy_nodejs.sdlc.html) `eb init`, `eb create`, `eb deploy`.

All dependencies are automatically taken care of.

Of course, your instance of Token will not have any content. Use the REST API to populate your Token with your favourite collection of logos.

## Organization
Token is divided in three main areas

### Server
Entry point: `bin/www`, which initializes `token-app.js`

The routes present in `routes/` are then deferred to. `logos.js` is responsible for the API, while `index.js` is responsible for the site.

### Logo feature extraction and comparison
This is Token’s primary functionality, written in C++ and located in `src/`. `logo_features.cc` integrates the feature extraction with MonogDB and provides the Node addon interface. `feature_extraction.cc` uses OpenCV to extract features from logos (`get_features`), and compares logo features (`feature_distance`). `beta_shape.cc` is used by `feature_extraction.cc` to calculate a logo’s [“beta-shape”](http://www.researchgate.net/publication/260074839_Beta-Shape_Using_Delaunay-Based_Triangle_Erosion), used to describe the general shape of a logo.

### Front end
Contained in `public/` and `views/`. Notably, `public/javascripts/token.js` holds the client-side logic.

## API
The logos in Token’s database are organized into *organizations*. As you would expect, a given organization can have multiple logos. Each logo still has a unique ID. Operations are provided for adding and deleting logos and organizations (with admin identification), fetching information on organizations, logos, and tags (which are associated with organizations), as well as searching for similar logos.

URL                   | Method | Parameters | Description | Returns 
----------------------|--------|------------|-------------|--------
`/r/org`                | `GET`    | `[page INTEGER]` | Returns `page` of a paged list of organizations. `page` defaults to zero. | JSON array of no more than 100 organizations
`/r/org`                | `GET`    | `[q STRING]` | Search for an organization matching the name `q` | JSON array of organizations
`/r/org`                | `POST*`   | `[org JSON]`* | Add a new organization | ID of the newly created organization
`/r/org/ORG_ID`         | `GET`    | | Information about an organization | JSON organization object
`/r/org/ORG_ID`         | `POST*`   | `[image FILE]` `[logo JSON]`** | Add a new logo | ID of the newly created logo
`/r/org/ORG_ID`         | `PUT*`   | `[org JSON]`* | Update a organization | Success
`/r/org/ORG_ID`         | `DELETE*` | | Delete the given organization | 
`/r/logo/LOGO_ID`       | `DELETE*` | | Delete the given logo | 
`/r/logo/LOGO_ID`       | `PUT*`    | `[image FILE]` `[logo JSON]`** | Update a logo | Success
`/r/logo/LOGO_ID`       | `GET`    | | Information about a logo | JSON logo object
`/r/search/`            | `POST`   | `[logo FILE]` | Search for logos matching the given file | JSON array of `[distance, logo_id, org_id]` tuples, sorted by increasing `distance`
`/r/search/`            | `GET`    | `[logo STRING]` | Search for logos matching the file at the given URL | JSON array of `[distance, logo_id, org_id]` tuples, sorted by increasing `distance`
`/r/tags/`              | `GET`    | | List tags | JSON array of tag strings
`/r/tags/TAG`           | `GET`    | | Organizations with the given `TAG` | JSON array of organization IDs
`/r/stats`              | `GET`    | | Information about Token instance | JSON object: `{orgs: N_ORGS, logos: N_LOGOS}`

\*: Requires basic access authentication for the user `admin`

\*\*: A JSON object with the following fields: `{name: STRING, tags: LIST_OF_STRINGS, [website: STRING, active: BOOL]}`

\*\*\*: A JSON object with the following fields: `{name: STRING, date: TIMESTAMP, [retrieved_from: STRING, active: BOOL]}`
