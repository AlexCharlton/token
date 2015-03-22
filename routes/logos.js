var express = require('express')
var mongojs = require('mongojs')
var shortid = require('shortid')
var multer  = require('multer')
var async  = require('async')
var mkdirp  = require('mkdirp')
var gm  = require('gm')
var fs = require('fs')
var http = require('http')
var path = require('path')
var _ = require('underscore')
var features
if (process.env['DEBUG']){
    features = require('../build/Debug/logo_features.node')
} else {
    features = require('../build/Release/logo_features.node')
}

var router = express.Router();
var database_server = process.env['TOKEN_DB_SERVER'] || 'localhost'
var database_name = process.env['TOKEN_DB'] || 'token'
var db = mongojs(database_server + "/" + database_name, 
                 ['tags', 'orgs', 'logos', 'features'])

var logo_src_base = '/logos/'
var logo_store_base = (process.env['AWS']) ? '/opt' : './public'
var logo_store = path.join(logo_store_base, logo_src_base)
var search_store = 'searches/'
var download_dir = 'downloads/'

db.features.createIndex({"aspect": 1})

var auth_string = new Buffer("admin:" + process.env['TOKEN_PASSWORD'])
auth_string = 'Basic ' + auth_string.toString('base64')
function not_admin(req, res){
    var auth = req.headers.authorization
    if (auth != auth_string){
        res.status(403).end()
        return true
    }
    return false
}

//// Organizations
// _id
// name
// tags (ids)
// website
// logos (ids)
// active?
// review?

var n_orgs_per_page = 100;
router.get('/org', function send_orgs(req, res, next) {
    var query = req.query.q
    if (query) {
        // Return matches of given query
        var r = new RegExp(query, 'i');
        db.orgs
            .find({name: r}, function(err, orgs){
                if (err){
                    console.error(err)
                    return
                }
                res.send(orgs);
            });
    } else {
        // Send chuncked list of orgs
        var page = parseInt(req.query.page) || 1;
        page = page < 1 ? 1 : page;
        db.orgs
            .find()
            .sort({name:1})
            .limit(n_orgs_per_page)
            .skip((page-1)*n_orgs_per_page, function(err, orgs){
                if (err){
                    console.error(err)
                    return
                }
                res.send(orgs);
            });
    }
});

router.get('/org/:org', function send_org(req, res, next) {
    // Send org
    db.orgs.find({_id: req.params.org}, function(err, orgs){
        if (err || orgs.length == 0) {
            console.error("No organization found: " + req.params.org);
            return res.status(404).send("No such organization: " + req.params.org);
        }
        var org = orgs[0];
        async.map(org.logos, get_logo, function(err, result){
            if (err){
                return res.status(500).end()
            }
            org.logos = result
            res.send(org)
        })
    });
});

router.post('/org', function create_org(req, res, next) {
    if (not_admin(req, res)) return
    // add new org
    var id = shortid.generate();
    var o = JSON.parse(req.body.org)
    var name = o.name;
    var tags = o.tags;
    var website = o.website;
    var active = o.active;
    active = (typeof active == 'undefined') ? true : active;
    if (!(name && tags )){
        console.error(req.body);
        return res.status(400).send("Missing form data in org creation " 
                                    + name + " " + " " + tags + " " + website);
    }
    var org = {_id: id, name: name, tags: tags, website: website,
               logos: [], active: active, review: false};
    tags.forEach(function(tag){ add_org_to_tag(id, tag); });
    db.orgs.insert(org, function(err, value){
        res.status(201).send(id);
    });
});

router.put('/org/:org', function update_org(req, res, next) {
    if (not_admin(req, res)) return
    // update org
    db.orgs.find({_id: req.params.org}, function(err, orgs){
        if (err || orgs.length == 0) {
            console.error("No organization found: " + req.params.org);
            return res.status(404).send("No such organization: " + req.params.org);
        }
        var org = orgs[0];
        var o = JSON.parse(req.body.org)
        if (o){
            if (o.name) org.name = o.name
            if (o.active) org.active = o.active
            if (o.review) org.review = o.review
            if (o.tags) org.tags = o.tags
            if (o.website) org.website = o.website
            db.orgs.update({_id: req.params.org}, 
                           {$set: ort});
        }
        res.status(204).end();
    });
});

router.delete('/org/:org', function rm_org(req, res, next) {
    if (not_admin(req, res)) return
    // Remove org
    db.orgs.find({_id: req.params.org}, function(err, orgs){
        if (err || orgs.length == 0) {
            console.error("No organization found: " + req.params.org);
            return res.status(404).send("No such organization: " + req.params.org);
        }
        var org = orgs[0];
        org.logos.forEach(function(logo){ rm_logo(logo, org._id) })
        db.orgs.remove({_id: org._id});
        fs.rmdir(path.join(logo_store, org._id))
        res.status(204).end();
        console.log("Deleted org: ", org._id);
    });
});

router.get('/flagged', function send_org(req, res, next) {
    // Send flagged orgs and logos
    db.orgs.find({review: true}, function(err, orgs){
        if (err) {
            return res.status(500).send("Cannot get flagged orgs");
        }
        db.logos.find({review: true}, function(err, logos){
            if (err) {
                return res.status(500).send("Cannot get flagged logos");
            }
            res.send({org: orgs, logo: logos});
        });
    });
});


//// Logos
// _id
// name
// file
// src
// date
// retrieved_from
// active?
// review?

function get_logo(logo, callback){
    db.logos.find({_id: logo}, function(err, logos){
        if (err || logos.length == 0) {
            console.error("No logo found: " + req.params.logo); 
            callback("No such logo: " + logo, null)
        } else {
            callback(null, logos[0])
        }
    })
}

router.get('/logo/:logo', function send_logo(req, res, next) {
    // Send logo
    db.logos.find({_id: req.params.logo}, function(err, logos){
        if (err || logos.length == 0) {
            console.error("No logo found: " + req.params.logo); 
            return res.status(404).send("No such logo: " + req.params.logo);
        }
        res.send(logos[0]);
    });
});

var shortest_max = 200
var hard_max = 400
function mv_resize_image(src, dest, succ, error){
    // Make sure smallest dimension isn't larger than resize_max
    mkdirp.sync(path.dirname(dest))
    gm(src)
        .size(function (err, size) {
            if (err) return error(err)
            var geom = {}
            if (size.width > size.height){
                geom.width = null
                geom.height = shortest_max
            } else {
                geom.width = shortest_max
                geom.height = null
            }
            gm(src)
                .resize(geom.width, geom.height, ">")
                .resize(hard_max, hard_max, ">")
                .write(dest, function(err){
                    if (err) return error(err)
                    console.log("Added image to store: ", dest)
                    fs.unlink(src)
                    succ()
                })
        });
}

router.post('/org/:org', function create_logo(req, res, next) {
    if (not_admin(req, res)) return
    // add new logo
    db.orgs.find({_id: req.params.org}, function(err, orgs){
        if (err || orgs.length == 0) {
            console.error("No organization found: " + req.params.org)
            return res.status(404).send("No such organization: " + req.params.org)
        }
        var org = orgs[0]
        var id = shortid.generate()
        var l = JSON.parse(req.body.logo)
        var file = req.files.image
        var name = l.name
        var date = l.date
        var retrieved_from = l.retrieved_from
        var active = l.active
        active = (typeof active == 'undefined') ? true : active
        active = (org.active) ? active : false
        name = (name) ? name : org.name
        file = (file) ? file[0] : file
        var review = false // TODO set based on match?
        if (!(file && date)){
            return res.status(400).send("Missing form data in logo creation: " + file + ' ' + date)
        }
        var file_path = path.join(logo_store,
                            org._id, 
                            id + '.png')
        var src = path.join(logo_src_base,
                            org._id, 
                            id + '.png')
        mv_resize_image(
            file.path, file_path, 
            function success(){
                var logo = {_id: id, file: file_path, src: src,
                            date: date, name: name, org: org._id,
                            retrieved_from: retrieved_from,
                            active: active, review: review}

                var abort = function(e){
                    console.error(e)
                    fs.unlink(file_path)
                    res.status(400).send(e)
                }
                db.logos.insert(logo, function(err, value){
                    if (err) return abort(err)
                    try {
                        features.extract(id,
                                         database_name + '.logos',
                                         database_name + '.features',
                                         database_server)
                        res.status(201).send(id)
                    } catch (e){ abort(e) }
                })
            },
            function(e){
                console.error(e)
                fs.unlink(file.path)
                res.status(400).send(e)
            }
        )
        db.orgs.update({_id: org._id},
                       {$push: {logos: id}})
    })
})

router.put('/logo/:logo', function update_logo(req, res, next) {
    if (not_admin(req, res)) return
    //update logo
    db.logos.find({_id: req.params.logo}, function(err, logos){
        if (err || logos.length == 0) {
            console.error("No logo found: " + req.params.logo)
            return res.status(404).send("No such logo: " + req.params.logo)
        }
        var logo = logos[0]
        var l = JSON.parse(req.body.logo)
        if (l){
            if (l.name) logo.name = l.name
            if (l.date) logo.date = l.date
            if (l.retrieved_from) logo.retrieved_from = l.retrieved_from
            if (l.active) logo.active = l.active
            if (l.review) logo.review = l.review
        }
        db.logos.update({_id: logo._id}, 
                        {$set: logo});

        var new_file = req.files.image
        new_file = (new_file) ? new_file[0] : new_file
        if (new_file){
            var abort = function(e){
                console.error(e)
                fs.unlink(new_file.path)
                res.status(400).send(e)
            }
            var temp_file = logo.path + '.tmp'
            mv_resize_image(
                new_file.path, temp_file, 
                function success(){
                    try {
                        features.extract(logo.id,
                                         database_name + '.logos',
                                         database_name + '.features',
                                         database_server)
                        fs.rename(temp_file, logo.path, function(err){
                            if (err){
                                console.error(err)
                                fs.unlink(temp_file)
                                res.status(400).send(err)
                            } else {
                                res.status(204).end();
                            }
                        })
                    } catch (e){
                        console.error(e)
                        fs.unlink(temp_file)
                        res.status(400).send(e)
                    }
                },
                function(e){
                    console.error(e)
                    fs.unlink(new_file.path)
                    res.status(400).send(e)
                }
            )
        } else {
            res.status(204).end();
        }
    })
})

function rm_logo(logo, org_id, cb){
    db.logos.find({_id: logo}, function(err, logos){
        if (err || logos.length == 0) {
            cb("Tried to delete logo, but wasn't found: " + logo)
        }
        try {
            fs.unlink(logos[0].file)
        } catch (e){
            console.error("Failed to delete logo file: ", logo, " ", logos[0].file)
        }
        db.logos.remove({_id: logo}, function(err){
            if (err) return cb(err)
            db.features.remove({_id: logo}, cb)
        })
    })
}

router.delete('/logo/:logo', function del_logo(req, res, next) {
    if (not_admin(req, res)) return
    // Remove logo
    var logo_id = req.params.logo;
    db.logos.find({_id: logo_id}, function(err, logos){
        if (err || logos.length == 0) {
            console.error("No logo found: " + logo_id);
            return res.status(404).send("No such logo: " + logo_id);
        }
        var logo = logos[0]
        db.orgs.find({_id: logo.org}, function(err, orgs){
            if (err || orgs.length == 0) {
                console.error("No organization found: " + logo.org);
                return res.status(404).send("No such organization: " + logo.org);
            }
            var org = orgs[0];
            var logo_list = _.without(org.logos, logo_id);
            if (logo_list.length == 0){
                // Org have no logos? Flag review
                db.orgs.update({_id: org._id}, {$set: {review: true}});
            }
            db.orgs.update({_id: org._id}, {$set: {logos: logo_list}});
            rm_logo(logo_id, org._id, function(err){
                if (err) {
                    console.error(err)
                    res.status(400).end();
                } else {
                    res.status(204).end();
                    console.log("Deleted logo: ", logo_id);
                }
            })
        })
    })
});


//// Search
var MAX_RESULTS = 500
var search_results = function(src, res){
    var dest = path.join(search_store, shortid.generate() + '.png')
    mv_resize_image(
        src, dest,
        function success(){
            try{
                var results = features.search(dest, 
                                              database_name + '.features',
                                              database_server)
                results.sort(function(a, b){ return a[0] - b[0] })
                results = results.slice(0, MAX_RESULTS)
                res.send(results)
            } catch (e) {
                console.error(e)
                res.status(400).send(e)
            }
            fs.unlink(dest)
        },
        function error(e){
            console.error(e)
            fs.unlink(src)
            res.status(400).send(e)
        })
}


router.post('/search', function send_results(req, res, next){
    var logo = req.files.logo
    if (logo.length == 0){
        return res.status(400).send("Missing logo in search");
    }
    search_results(logo[0].path, res)
})

var download = function(url, dest, cb) {
    mkdirp.sync(path.dirname(dest))
    var file = fs.createWriteStream(dest);
    var request = http.get(url, function(response) {
        response.pipe(file);
        file.on('finish', function() {
            file.close(cb);
        });
    }).on('error', function(err) {
        fs.unlink(dest);
        if (cb) cb(err.message);
    });
};

router.get('/search', function send_results(req, res, next){
    var logo = req.query.logo
    var download_path = path.join(download_dir, shortid.generate())
    download(logo, download_path, function(err){
        if (err){
            console.error(err)
            return res.status(400).send(e)
        }
        search_results(download_path, res)
    })
})


//// Tags
// _id (name)
// orgs (tags)

function rm_org_from_tags(org, tags){
    db.tags.find({_id: tags}, function(err, tags){
        if (err){
            console.error(err)
            return
        }
        tags.forEach(function(tag){
            var orgs = _.without(tag.orgs, org);
            if (orgs.length == 0) {
                db.tags.remove({_id: name});
            } else {
                db.tags.update({_id: tag}, {$set: {orgs: orgs}});
            }
        });
    });
};

function add_org_to_tag(org, tag){
    db.tags.update({_id: tag},
                   {$push: {orgs: org}},
                   {upsert: true});
};

router.get('/tags', function send_tags(req, res, next){
    // Send list of tags
    db.tags.find({}, {_id:1}).sort({_id:1}, function(err, tags){
        if (err || tags.length == 0) {
            console.error("No tags found");
            return res.status(404).send("No tags found");
        }
        res.send(tags);
    });
});

router.get('/tags/:tag', function send_tag(req, res, next){
    // Send organizations in tag
    db.tags.find({_id: req.params.tag}, function(err, tags){
        if (err || tags.length == 0) {
            console.error("No tag found: " + req.params.tag);
            return res.status(404).send("No such tag: " + req.params.tag);
        }
        res.send(tags[0]);
    });
});

//// Stats
router.get('/stats', function send_tag(req, res, next){
    // Send statistics about site
    db.logos.stats(function(err, logo_stats){
        db.orgs.stats(function(err, org_stats){
            res.send({orgs: org_stats.count,
                      logos: logo_stats.count});

        })
    })
})

module.exports = router;
