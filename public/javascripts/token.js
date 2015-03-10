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

api.factory('Org', ['$resource', '$location', function($resource, $location){
    var Org = $resource('/r/org/:org/') 
    var match = /\/org\/([\w\-]+)/g.exec($location.path())
    var org_id = match ? match[1] : '404'

    var logo = function(org){
        var logo_id = $location.hash()
        var logo = null
        if (logo_id) {
            logo = _.find(org.logos, function(l){ 
                return l._id == logo_id
            })
        }
        if (!logo && !_.isEmpty(org.logos)){
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
        var org = Org.get(
            {org: org_id}, 
            function get_org(){ callback(org) },
            error_callback)
    }

    return {get:get, logo:logo}
}])

api.factory('Search', ['$http', '$resource', function ($http, $resource){
    var Search = $resource('/r/search') 

    var distance_sort = function(a, b){
        return a.distance - b.distance
    }

    var process_results = function(r){
        a = _.values(r.results).sort(distance_sort)
        r.results = a
        return r
    }

    var upload = function(file, cb, err){
        var fd = new FormData()
        fd.append('logo', file)
        $http.post('/r/search', fd, {
            transformRequest: angular.identity,
            headers: {'Content-Type': undefined}
        })
            .success(function(res){
                cb(process_results(res))
            })
            .error(function(res){
                err(res)
            })
    }

    var url = function(url, callback, error_callback){
        var results = Search.get(
            {logo: url}, 
            function search(){
                callback(process_results(results))
            },
            error_callback)
    }

    return { uploadLogoSearch: upload,
             urlSearch: url }
}])

app.controller('Main', ['$scope', 'Stats', 'Search', function($scope, Stats, Search){
    $scope.logos = "no search"
    var display_results = function(results){
        $scope.search = results.id
        if (_.isEmpty(results.results)){
            $scope.logos = "none"
        } else {
            $scope.logos = results.results
        }
    }

    var search_error = function(results){
        $scope.logos = "error"
        console.error(results)
    }

    $scope.logoSearch = function(url){
        var file = $scope.logo
        if (file){
            Search.uploadLogoSearch(file, display_results, search_error)
        } else {
            Search.urlSearch(url, display_results, search_error)
        }
    }

    Stats(function(result){
        $scope.n_logos = result.logos
        $scope.n_orgs = result.orgs
    })
}])

app.controller(
    'Organization', 
    ['$scope', '$window', 'Org', 
     function($scope, $window, Org){
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

app.directive('fileModel', ['$parse', function ($parse) {
    return {
        restrict: 'A',
        link: function(scope, element, attrs) {
            var model = $parse(attrs.fileModel)
            var modelSetter = model.assign
            
            element.bind('change', function(){
                scope.$apply(function(){
                    modelSetter(scope, element[0].files[0])
                })
            })
        }
    }
}])
