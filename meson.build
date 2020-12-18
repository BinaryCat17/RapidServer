project('RapidServer', 'cpp',
    version : '1.0',
    default_options : ['cpp_std=c++2a', 'warning_level=3'])

cmake = import('cmake')
opt_var = cmake.subproject_options()
opt_var.add_cmake_defines({'BUILD_TESTING': false, 'BUILD_EXAMPLES': false, 'SQLITE_ORM_ENABLE_CXX_17': true})

uWebSocketsMeson = dependency('uWebSocketsMeson', fallback : ['uWebSocketsMeson', 'uWebSocketsMeson_dep'])
threads = dependency('threads')

sqlite_orm = dependency('SqliteOrm', required : false)
if not sqlite_orm.found()
    sqlite_orm = cmake.subproject('sqlite_orm', options: opt_var).dependency('sqlite_orm')
endif

deps = [threads, uWebSocketsMeson, sqlite_orm]
includes = 'src'

lib = static_library('RapidLib',
    'src/rapid/utils.cpp',
    dependencies : deps,
    include_directories : includes)

executable('app', 'app/main.cpp', include_directories : includes, dependencies : deps, link_with : lib)