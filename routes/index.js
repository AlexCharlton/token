var express = require('express');
var router = express.Router();

router.get('/', function(req, res, next) {
    res.render('index');
});

// router.get('/about', function(req, res, next) {
//     res.render('about');
// });

// router.get('/tags', function(req, res, next) {
//     res.render('index', { title: 'Token' });
// });

// router.get('/tags/:tag', function(req, res, next) {
//     // Chunk results
//     res.render('index', { title: 'Token' });
// });

// router.get('/org', function(req, res, next) {
//     // Results are chuncked
//     res.render('index', { title: 'Token' });
// });

// router.get('/org/:org', function(req, res, next) {
//     res.render('logo');
// });

module.exports = router;
