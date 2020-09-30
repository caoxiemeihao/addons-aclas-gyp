{
  "targets": [
    {
      "target_name": "aclas_c",
      "sources": [ "src/aclas_c.c" ],
    },
    {
      "target_name": "aclas_cc",
      "sources": [ "src/aclas_cc.cc" ],
      "include_dirs": [
        "<!@(node -p \"require('node-addon-api').include\")"
      ],

      # optional
      "cflags!": [ "-fno-exceptions" ],
      "cflags_cc!": [ "-fno-exceptions" ],

      # fatal error C1189: #error:  Exception support not detected.
      # Define either NAPI_CPP_EXCEPTIONS or NAPI_DISABLE_CPP_EXCEPTIONS.
      "defines": [ 'NAPI_DISABLE_CPP_EXCEPTIONS' ],
    },
  ]
}