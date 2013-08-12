def options(opt):
	opt.load("compiler_c")
	opt.add_option("--debug",action="store_true",default=False,help="Debug traces")

def configure(cnf):
	cnf.load("compiler_c")
	cnf.env.append_value("CFLAGS",["-Wall"])

	if cnf.options.debug:
		cnf.env.append_value("CFLAGS",["-DDEBUG"])

	cnf.check(
		features='c cprogram', 
		lib=[
			'avcodec', 'avformat', 'iconv', 'm', 'mp3lame', 'bz2', 'z', 'm', 'avutil'
		],
		uselib_store='AV'
	)

def build(bld):
	bld(
		features="c cprogram", 
		source='src/ffsndtest.c src/ffsndin.c src/ffsndout.c src/ffsndutil.c', 
		target='ffsndtest', 
		use=['AV']
	)