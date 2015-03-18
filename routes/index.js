var express = require('express');
var router = express.Router();

router.get('/', function(req, res, next) {
    res.render('index');
});

// router.get('/org/:org', function(req, res, next) {
//     res.render('logo');
// });

module.exports = router;
