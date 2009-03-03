
test1:
	python facquire.py -S10 images/ntfs1-gen2.dd samples/mediumimage

clean:
	rm -f samples/test*zip samples/medium*zip samples/small*zip