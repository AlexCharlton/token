var express = require('express');
var router = express.Router();

// Verify admin password
router.get('/', function(req, res, next) {
    // admin cookie?
      // render login
    // else
      // link to org, logo pages
    res.render('index', { title: 'Token' });
});

router.get('/org', function(req, res, next) {
    res.render('index', { title: 'Token' });
});

router.get('/logo', function(req, res, next) {
    res.render('index', { title: 'Token' });
});

router.get('/org/:org', function(req, res, next) {
    res.render('index', { title: 'Token' });
});

router.get('/org/:org/:logo', function(req, res, next) {
    res.render('index', { title: 'Token' });
});

module.exports = router;
