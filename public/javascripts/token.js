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

app.controller('Main', ['$scope', 'Stats', function($scope, Stats){
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
