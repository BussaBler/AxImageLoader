Import('env', 'buildInfo')

platform = buildInfo['platform']
architecture = buildInfo['architecture']
compiler = buildInfo['compiler']
config = buildInfo['config']
configName = config.capitalize()
vsproj = buildInfo['vsproj']
renderer = buildInfo['renderer']

localEnv = env.Clone()

localEnv.Append(CPPPATH=[
    "Include"
])

sources = Glob('Source/*.cpp')

axImageLoader = localEnv.StaticLibrary(f'#/Bin/{configName}/AxImageLoader/AxImageLoader', sources)

Return('axImageLoader')