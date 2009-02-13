from hotshot import stats

s = stats.load("hotshot_edi_stats")
s.sort_stats("time").print_stats()
