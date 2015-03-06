var express = require('express');
var router = express.Router();
var mongojs = require('mongojs');
var db = mongojs('token', ['tags', 'orgs', 'logos']);
var shortid = require('shortid');
var multer  = require('multer');
var async  = require('async');
var mkdirp  = require('mkdirp');
var gm  = require('gm');
var fs = require('fs')
var path = require('path')
var _ = require('underscore');

var image_store = 'img_store'
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
                // TODO check err
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
                // TODO check err
                res.send(orgs);
            });
    }
});

router.get('/org/:org', function send_org(req, res, next) {
    // Send org
    db.orgs.find({_id: req.params.org}, function(err, orgs){
        if (err || orgs.length == 0) {
            // TODO proper logging
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
    // add new org
    var id = shortid.generate();
    var o = JSON.parse(req.body.org)
    var name = o.name;
    var tags = o.tags;
    var website = o.website;
    var active = o.active;
    active = (typeof active == 'undefined') ? true : active;
    if (!(name && tags )){
        console.error(req.body); // TODO log
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
    // update org
    db.orgs.find({_id: req.params.org}, function(err, orgs){
        if (err || orgs.length == 0) {
            // TODO proper logging
            console.error("No organization found: " + req.params.org);
            return res.status(404).send("No such organization: " + req.params.org);
        }
        var org = orgs[0];
        var old_tags = org.tags;
        var name = req.params.name || org.name;
        var tags = req.params.tags || org.tags;
        var website = req.params.tags || org.website;
        var active = req.params.active;
        var review = false; // TODO set based on name match?
        active = (typeof active == 'undefined') ? org.active : active;
        review = (typeof review == 'undefined') ? org.review : review;
        rm_org_from_tags(name, _.difference(old_tags, tags));
        db.orgs.update({_id: req.params.org}, 
                       {$set: {name: name,
                               tags: tags,
                               website: website,
                               active: active,
                               review: review}});
        res.status(204).end();
    });
});

router.delete('org/:org', function rm_org(req, res, next) {
    // Remove org
    db.orgs.find({_id: req.params.org}, function(err, orgs){
        if (err || orgs.length == 0) {
            // TODO proper logging
            console.error("No organization found: " + req.params.org);
            return res.status(404).send("No such organization: " + req.params.org);
        }
        var org = orgs[0];
        org.logos.forEach(function(logo){ rm_logo(logo, org._id) });
        db.orgs.remove({_id: org._id});
        // TODO remove image folder from store
        res.status(204).end();
    });
});

router.get('/flagged', function send_org(req, res, next) {
    // Send flagged orgs and logos
    db.orgs.find({review: true}, function(err, orgs){
        if (err) {
            // TODO proper logging
            return res.status(500).send("Cannot get flagged orgs");
        }
        db.logos.find({review: true}, function(err, logos){
            if (err) {
                // TODO proper logging
                return res.status(500).send("Cannot get flagged logos");
            }
            res.send({org: orgs, logo: logos});
        });
    });
});


//// Logos
// _id
// file
// date
// features
// active?
// review?

function get_logo(logo, callback){
    db.logos.find({_id: logo}, function(err, logos){
        if (err || logos.length == 0) {
            // TODO proper logging
            console.error("No logo found: " + req.params.logo); 
            callback("No such logo: " + logo, null)
        } else {
            callback(null, _.omit(logos[0], 'features'))
        }
    })
}

router.get('/org/:org/:logo', function send_logo(req, res, next) {
    // Send logo
    db.logos.find({_id: req.params.logo}, function(err, logos){
        if (err || logos.length == 0) {
            // TODO proper logging
            console.error("No logo found: " + req.params.logo); 
            return res.status(404).send("No such logo: " + req.params.logo);
        }
        res.send(_.omit(logos[0], 'features'));
    });
});

var resize_max = 200
function mv_resize_image(src, dest){
    // Make sure smallest dimension isn't larger than resize_max
    mkdirp.sync(path.dirname(dest))
    gm(src)
        .size(function (err, size) {
            if (err) return console.error(err)
            var geom = {}
            if (size.width > size.height){
                geom.width = null
                geom.height = resize_max
            } else {
                geom.width = resize_max
                geom.height = null
            }
            gm(src)
                .resize(geom.width, geom.height, ">")
                .write(dest, function(err){
                    if (err) return console.error(err)
                    console.log("Added image to store: ", dest)
                    fs.unlink(src)
                })
        });
}

router.post('/org/:org', function create_logo(req, res, next) {
    // add new logo
    db.orgs.find({_id: req.params.org}, function(err, orgs){
        if (err || orgs.length == 0) {
            // TODO proper logging
            console.error("No organization found: " + req.params.org);
            return res.status(404).send("No such organization: " + req.params.org);
        }
        var org = orgs[0]
        var id = shortid.generate();
        var l = JSON.parse(req.body.logo)
        var file = req.files.image;
        var name = l.name;
        var date = l.date;
        var retrieved_from = l.retrieved_from;
        var features = false; // TODO 
        var active = l.active;
        active = (typeof active == 'undefined') ? true : active;
        active = (org.active) ? active : false
        name = (name) ? name : org.name;
        console.log(file)
        file = (file) ? file[0] : file
        var review = false; // TODO set based on match?
        if (!(file && date)){
            return res.status(400).send("Missing form data in logo creation: " + file + ' ' + date);
        }
        var src = '/' + path.join(image_store, org._id, file.name)
        mv_resize_image(file.path, './public' + src)
        var logo = {_id: id, file: src, date: date, features: features,
                    name: name, org: org._id, retrieved_from: retrieved_from,
                    active: active, review: review};
        db.orgs.update({_id: org._id},
                       {$push: {logos: id}});
        db.logos.insert(logo, function(err, value){
            res.status(201).send(id);
        });
    });
});

router.put('/org/:org/:logo', function update_logo(req, res, next) {
    //update logo
    db.logos.find({_id: req.params.logo}, function(err, logos){
        if (err || logos.length == 0) {
            // TODO proper logging
            console.error("No logo found: " + req.params.logo);
            return res.status(404).send("No such logo: " + req.params.logo);
        }
        var logo = logos[0];
        // if new file
          // req.files
          // TODO convert logo to uniform size, type; save
          // var features = features();
        // else
        var features = false;
        var file = req.params.file || logo.file;
        var date = req.params.date || logo.date;
        var active = req.params.active;
        var review = false; // TODO set based on match?
        active = (typeof active == 'undefined') ? logo.active : active;
        review = (typeof review == 'undefined') ? logo.review : review;
        db.logos.update({_id: logo._id}, 
                       {$set: {file: file,
                               date: date,
                               features: features,
                               active: active,
                               review: review}});
        res.status(204).end();
    });
});

function rm_logo(logo, org_id){
    db.logos.remove({_id: logo});
    // TODO remove image from store
}

router.delete('org/:org/:logo', function rm_logo(req, res, next) {
    // Remove logo
    var logo = req.params.logo;
    db.orgs.find({_id: req.params.org}, function(err, orgs){
        if (err || orgs.length == 0) {
            // TODO proper logging
            console.error("No organization found: " + req.params.org);
            return res.status(404).send("No such organization: " + req.params.org);
        }
        var org = orgs[0];
        var logos = _.without(org.logos, logo);
        if (logos.length == 0){
            // Org have no logos? Flag review
            db.orgs.update({_id: org._id}, {$set: {review: true}});
        }
        db.orgs.update({_id: org._id}, {$set: {logos: logos}});
        rm_logo(logo, req.params.org);
        res.status(204).end();
    })
});



//// Tags
// _id (name)
// orgs (tags)

function rm_org_from_tags(org, tags){
    db.tags.find({_id: tags}, function(err, tags){
        if (err) return; // TODO log
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
            // TODO proper logging
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
            // TODO proper logging
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
