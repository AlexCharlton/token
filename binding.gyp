{
    'targets': [
        {
            'target_name': 'logo_features',
            'sources': ['src/logo_features.cc'],
            "include_dirs" : ["<!(node -e \"require('nan')\")"],
            'cflags': [
                '<!@(pkg-config --cflags opencv)',
                '-std=c++0x'
            ],
            'cflags_cc!': [ '-fno-rtti', # enable rtti
                            '-fno-exceptions' # enable exceptions
                        ],
            'libraries': [
                '<!@(pkg-config --libs opencv)',
                '-lpthread',
                '-lmongoclient',
                '-lboost_thread-mt -lboost_system -lboost_regex',
                '-lrt'
            ]
        }
    ]
}
