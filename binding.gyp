{
    'target_defaults': {
        'cflags': [
            '<!@(pkg-config --cflags opencv)',
            '-std=c++0x',
            '-Wno-deprecated-declarations',
            '-O3'
        ],
        'cflags_cc!': [ '-fno-rtti', # enable rtti
                        '-fno-exceptions' # enable exceptions
                    ],
        'libraries': [
            '<!@(pkg-config --libs opencv)',
        ]
    },
    'targets': [
        {
            'target_name': 'logo_features',
            'sources': ['src/logo_features.cc'],
            "include_dirs" : ["<!(node -e \"require('nan')\")"],
            'libraries': [
                '-lpthread',
                '-lmongoclient',
                '-lboost_thread-mt -lboost_system -lboost_regex',
                '-lrt'
            ]
        },
        {
            'target_name': 'test',
            'type': 'executable',
            'sources': ['src/test.cc']
        }
    ]
}
