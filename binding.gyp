{
    'targets': [
        {
            'target_name': 'logo_features',
            'sources': ['src/logo_features.cc'],
            "include_dirs" : ["<!(node -e \"require('nan')\")"],
            'cflags': [
                '<!@(pkg-config --cflags opencv)',
            ],
            'cflags_cc!': [ '-fno-rtti', # enable rtti
                            '-fno-exceptions' # enable exceptions
                          ],
            'libraries': [
                '<!@(pkg-config --libs opencv)',
                '-lpthread -lmongoclient -lboost_thread-mt -lboost_system -lboost_regex -lrt'
            ],
        }
        # {
        #     'target_name': 'feature_search',
        #     'sources': ['feature_search.cc'],
        #     'include_dirs': 'logo_features'
        # }
        
    ]
}
