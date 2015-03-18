var api = angular.module('token_api', ['ngResource'])
var app = angular.module('token', ['token_api'])

api.config(function($locationProvider) {
    $locationProvider.html5Mode(true)
})

api.factory('Stats', ['$resource', function($resource){
    return function(callback){
        $resource('/r/stats').get({}, callback)
    }
}])

api.factory('OrgPage', ['$resource', '$location', function($resource, $location){
    var Org = $resource('/r/org/:org')

    var logo = function(org){ // Which logo is currently active?
        var logo_id = $location.hash()
        var logo = null
        if (logo_id) { // The URL is specifing the logo
            logo = _.find(org.logos, function(l){ 
                return l._id == logo_id
            })
        }
        if (!logo && !_.isEmpty(org.logos)){ // Get most recent active logo
            var active_not = _.partition(org.logos, function(l){ 
                return l.active 
            })
            var logos = _.isEmpty(active_not[0]) ? active_not[1] : active_not[0]
            logos.sort(function(a,b){
                return new Date(b.date) - new Date(a.date);
            })
            logo =  logos[0]
        }
        return logo
    }

    var get = function(callback, error_callback){
        var match = /\/org\/([\w\-]+)/g.exec($location.path())
        var org_id = match ? match[1] : '404'
        var org = Org.get(
            {org: org_id}, 
            function get_org(){ callback(org) },
            error_callback)
    }

    return {get: get, logo: logo}
}])

api.factory('Logo', ['$resource', 'Org', function($resource, Org){
    var Logo = $resource('/r/logo/:logo') 

    var logo = function(logo_id, callback, error_callback){
        var l = Logo.get(
            {logo: logo_id}, 
            function get_logo(){
                Org(l.org, 
                    function(org){
                        l.org = org
                        callback(l)
                    },
                    error_callback)
            },
            error_callback)
    }

    return logo
}])

api.factory('Org', ['$resource', function($resource){
    var Org = $resource('/r/org/:org') 

    var org = function(org_id, callback, error_callback){
        var o = Org.get(
            {org: org_id}, 
            function get_logo(){ callback(o) },
            error_callback)
    }

    return org
}])

api.factory('Search', ['$http', '$resource', function ($http, $resource){
    var Search = $resource('/r/search') 

    var process_results = function(r){
        var i = 0
        return _.map(r, function(a){ 
            ret = {distance: a[0],
                   index: i,
                   _id: a[1],
                   org: a[2]}
            i += 1
            return ret
        })
    }

    var upload = function(file, cb, err){
        var fd = new FormData()
        var url = URL.createObjectURL(file)
        fd.append('logo', file)
        $http.post('/r/search', fd, {
            transformRequest: angular.identity,
            headers: {'Content-Type': undefined}
        })
            .success(function(res){
                cb({logos: process_results(res),
                    search: {src: url}})
            })
            .error(function(res){
                err(res)
            })
    }

    var url_search = function(url, cb, err){
        console.log("url search")
        var res = Search.query(
            {logo: url},
            function search(){
                cb({logos: process_results(res),
                    search: {src: url}})
            },
            function error(){
                console.log("error")
                err()
            })
    }

    return { uploadLogoSearch: upload,
             urlSearch: url_search }
}])

app.controller('Main', ['$scope', 'Stats', 'Search', 'Logo', function($scope, Stats, Search, Logo){
    var display_logo = function(result){
        Logo(result._id,
             function(logo){
                 logo.distance = result.distance
                 $scope.logo = logo
             },
             function(err){
                 console.error(err)
             })
    }
    $scope.display_logo = display_logo
    
    $scope.logos = "no search"
    var display_results = function(results){
        $scope.search = results.search
        if (_.isEmpty(results.logos)){
            $scope.logos = "none"
        } else {
            display_logo(results.logos[0])
            $scope.logos = results.logos
        }
    }

    var search_error = function(results){
        $scope.logos = "error"
        console.error(results)
    }

    $scope.logoSearch = function(url){
        if (url){
            $scope.logos = "searching"
            Search.urlSearch(url, display_results, search_error)
        } else if ($scope.logo){
            $scope.logos = "searching"
            Search.uploadLogoSearch($scope.logo,
                                    display_results,
                                    search_error)
        }
    }

    Stats(function(result){
        $scope.n_logos = result.logos
        $scope.n_orgs = result.orgs
    })
}])

app.controller(
    'Organization', 
    ['$scope', '$window', 'OrgPage', 
     function($scope, $window, OrgPage){
         Org.get(
             function(result){ 
                 $scope.org = result
                 $scope.logo = Org.logo(result)
                 $scope.$watch(function () {
                     return location.hash
                 }, function (value) {
                     $scope.logo = Org.logo(result)
                 });
             },
             function(err){ $window.location.href = '/404' })
     }])

app.directive('justifiedGallery', ['$window', function($window){
    link = function (scope, element, attrs){
        var min = 150
        var logos_per_row

        function getRow(index){
            return Math.floor(index / logos_per_row)
        }

        function moveBm(){
            var b = $('#bookmark')
            if (b){
                var h = b.parent().parent().height()
                b.css("top", "-" + (h + 13) + "px")
            }
        }

        function addBookmark(el){
            $('#bookmark').remove()
            var b = $("<img id='bookmark', src='/public/images/bookmark.svg'>")
            b.appendTo(el)
            moveBm()
        }

        function moveMatch(e){
            $(e).parent().before($('#match'))
            addBookmark($(e))
        }

        function constructGallery(logos){
            logos_per_row = Math.floor(element.width() / min)
            var width = element.width() / logos_per_row
            var rows = Math.ceil(logos.length / logos_per_row)
            var i = 0
            _(rows).times(function(r){
                var row = $('<div class="row logo-row">')
                row.appendTo(element)
                _(logos_per_row).times(function(l){
                    if (i == logos.length) return
                    var logo = logos[i]
                    var wrapper = $('<div class="result-wrapper">')
                    var img = $('<img src="/public/logos/' + 
                                logo.org + '/' + logo._id+ '.png">')
                    wrapper.width(width)
                    img.width(width)
                        .appendTo(wrapper)
                        .on("load", function(){
                            var i = $(this)
                            i.parent().height(i.height())
                            moveBm()
                        })
                    wrapper.appendTo(row)
                        .on("click", function(e) {
                            scope.display_logo(logo)
                            moveMatch(this)
                        })
                    if (i == 0) addBookmark(wrapper)
                    i += 1
                })
            })
        }

        scope.$watch(attrs.justifiedGallery, function(value){
            constructGallery(value)
        })
    }

    return {link: link,
            restrict: 'A'}
}])

app.directive('fileModel', ['$parse', function ($parse) {
    return {
        restrict: 'A',
        link: function(scope, element, attrs) {
            var model = $parse(attrs.fileModel)
            var modelSetter = model.assign
            
            element.bind('change', function(){
                scope.$apply(function(){
                    modelSetter(scope, element[0].files[0])
                    scope.logoSearch(null)
                })
                
            })
        }
    }
}])

app.filter("isActive", function(){
    return function(activep){
        return (activep) ? "Active" : "Inactive"
    }
})

app.filter("matchPercent", function(){
    return function(distance){
        return Math.max(0, Math.round(100 - distance*100))
    }
})
