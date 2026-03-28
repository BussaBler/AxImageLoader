Import('env')

localEnv = env.Clone()

localEnv.Append(CPPPATH=[
    'Include'
])

sources = localEnv.Glob('Source/*.cpp')

axImageLoaderLib = localEnv.StaticLibrary('axImageLoader', sources)

Return('axImageLoaderLib')