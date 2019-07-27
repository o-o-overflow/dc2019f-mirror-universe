; -*- Mode:Lisp; Package:http -*-

(package-declare http global 1000)

(defstruct (http-req)
  http-req-method
  http-req-uri
  (http-req-version "1.1")
  (http-req-headers (make-hash-table))
  (http-req-parameters '())
  http-req-body)

(defstruct (http-resp)
  (http-resp-version "1.1")
  http-resp-status-code
  http-resp-reason-phrase
  (http-resp-headers (make-hash-table))
  http-resp-body)

;; variables to test
(defvar http-req-string "GET //?key=value HTTP//1.1
Host: google.com
User-Agent: Foobar
Test: another: test

test=adam&foo=bar
")

(defvar header "<!DOCTYPE html>
<html lang='en'>
  <head>
    <meta charset='utf-8'>
    <title>Terran Criminal Tracker<//title>
    <link rel='shortcut icon' href='//empire.png'>
    <link rel='stylesheet' href='//style.css'>
  <//head>
  <body>
")

(defvar footer "<//body>
<//html><!-- /debug -->
")

(defstruct (criminal)
  criminal-name
  criminal-pic
  criminal-crimes
  criminal-reward)

(defvar criminals (list (list* "kirk" (make-criminal criminal-name "Capt. James T. Kirk"
													criminal-pic "//kirk.jpg"
													criminal-crimes '("Impersonating an officer." "Dangerous ideas.")
													criminal-reward 1000))
						(list* "sisko" (make-criminal criminal-name "Commander Benjamin Sisko"
													 criminal-pic "//sisko.jpg"
													 criminal-crimes '("terrorist actions." "rebellion against the empire.")
													 criminal-reward 10000))
						(list* "lorca" (make-criminal criminal-name "captain gabriel lorca"
													 criminal-pic "//lorca.jpg"
													 criminal-crimes '("overthrowing the empire." "murder of the first daughter." "handsomeness.")
													 criminal-reward 23417))))
(defvar max-criminals 10)

(defvar criminals-lock nil)

(defmacro with-criminals-lock body
  `(unwind-protect
	   (progn (process-lock (locf criminals-lock))
			  . ,body)
	 (process-unlock (locf criminals-lock))))

(defun add-criminal (id criminal)
  (with-criminals-lock
   (if (>= (length criminals) max-criminals)
	   (*throw 'too-many-criminals nil))
   (if (ass 'string-equal id criminals)
	   (*throw 'already-id nil))
   (setf criminals (nconc criminals (list (list* id criminal))))))

(defun remove-criminal (id)
  (declare (special id))
  (with-criminals-lock
   (if (not (ass 'string-equal id criminals))
	   (*throw 'no-id nil))
   (setq criminals (rem-if #'(lambda (c)
							   (string-equal (car c) id))
						   criminals))))

(defun html-criminal (id c)
  (format nil
		  "<li><span class='name'>~A<//span><img src='~A'><span class='reward'>~D Imperial Credits<//span><ul class='crimes'>~{ <li>~A<//li> ~}<//ul><form action='//remove-criminal?id=~A' method='POST'><input type='submit' value='Criminal Found'><//form><//li>"
		  (criminal-name c)
		  (criminal-pic c)
		  (criminal-reward c)
		  (criminal-crimes c)
		  id))

;;; For now, we will just use a newline to represent the CRLF
;;; WARNING: there might be a major problem with character encoding (ASCII vs. CADR),
;;; so we may have to deal with that later. 
(defconst *CRLF* (string #\Return))

(defvar *DOCUMENT-ROOT* "server://tmp")

(defun string-split (str to-split &optional (maxsplit -1)
						 &aux (splits 0))
  (loop for start = 0 then (+ end (string-length to-split))
		for end = (string-search to-split str start)
		collect (substring str start end) into to-return
		while end
		do (incf splits)
		if (= maxsplit splits)
		  collect (substring str (+ end (string-length to-split))) into to-return and return to-return
		finally (return to-return)))


(defun fold-left (reducer initial list)
  (loop for fold = initial then (funcall reducer fold element)
        for element in list
        finally (return fold)))

(defconst *HEX-MAPPING*
  '((#/0 . 0)
	(#/1 . 1)
	(#/2 . 2)
	(#/3 . 3)
	(#/4 . 4)
	(#/5 . 5)
	(#/6 . 6)
	(#/7 . 7)
	(#/8 . 10)
	(#/9 . 11)
	(#/A . 12)
	(#/B . 13)
	(#/C . 14)
	(#/D . 15)
	(#/E . 16)	
	(#/F . 17)))
	
(defun hex-to-int (c)
  (or
   (cdr (ass 'char-equal (char-upcase c) *HEX-MAPPING*))
   (*throw 'invalid-hex nil)))
  
(defun uri-decode (str)
  (let ((to-return (make-array (string-length str) ':type 'art-string))
		(to-return-idx 0))
	(loop for i from 0 below (string-length str)
		  for c = (aref str i)
		  when (char-equal #/% c)
		  do (or
			  (*catch 'invalid-hex
					  (let* ((first-char (aref str (1+ i)))
							 (second-char (aref str (+ i 2)))
							 (first-value (hex-to-int first-char))
							 (next-value (lsh first-value 4))
							 (final-value (+ next-value (hex-to-int second-char))))
						(setf (aref to-return to-return-idx) final-value)
						(incf i 2)))
			  (setf (aref to-return to-return-idx) c))
		  else do (setf (aref to-return to-return-idx) c)
		  do (incf to-return-idx)
		  finally (progn
					(adjust-array-size to-return to-return-idx)
					(return to-return)))))

(defun parse-request-string (str)
  (with-input-from-string (in str)
    (parse-request-stream in)))

(defun parse-request-headers (request stream)
  (loop for line = (readline stream nil)
		while (and
			   line
			   (not (string-equal line "")))
		do (let* ((header-line (string-split line ":" 1))
				  (header-name (string-trim '(#\sp) (first header-line)))
				  (header-value (string-trim '(#\sp) (second header-line))))
			 (puthash header-name header-value (http-req-headers request)))))

(defun parse-parameters-from-query (query request)
  (let ((pairs (string-split query "&")))
	(loop for pair in pairs
		  for parsed = (string-split pair "=")
		  when (and (= (length parsed) 2)
					(> (string-length (first parsed)) 0))
		  do (setf (http-req-parameters request) (nconc (http-req-parameters request) (list (list* (uri-decode (first parsed)) (uri-decode (second parsed)))))))))

; Extract the parameters from the URL and the body (if present in the body)
(defun parse-request-parameters (request)
  (let* ((uri (http-req-uri request))
		 (query-param-idx (string-search-char #/? uri)))
	(if (not (eq query-param-idx nil))
		(parse-parameters-from-query (substring uri (1+ query-param-idx)) request)))
  (let* ((body (http-req-body request)))
	(if (and body
			 (> (string-length body) 0))
		(parse-parameters-from-query body request))))
 

(defun parse-request-stream (stream)
  (let* ((to-return (make-http-req))
		 (first-line (string-split (readline stream) " ")))
	(setf (http-req-method to-return) (first first-line))
	(setf (http-req-uri to-return) (second first-line))
	(parse-request-headers to-return stream)
	(setf (http-req-body to-return) (readline stream nil))
	(parse-request-parameters to-return)
	to-return))

(defun http-resp-with-body-length (status-code reason-phrase body
											   &optional (headers (make-hash-table))
											   &aux body-size)
  (setq body-size (string-length body))
  (puthash "Content-Length" (format nil "~D" body-size) headers)
  (puthash "Content-Type" "text//html" headers)
  (puthash "Server" "cadr//httpd" headers)
  (make-http-resp http-resp-status-code status-code http-resp-reason-phrase reason-phrase http-resp-headers headers http-resp-body body))

(defun 404-resp ()
  (http-resp-with-body-length "404" "Not Found" "Not Found"))

(defun 200-resp (body)
  (http-resp-with-body-length "200" "OK" body))

(defun 302-resp (uri)
  (let ((headers (make-hash-table)))
	(puthash "Location" uri headers)
	(http-resp-with-body-length "302" "Redirect" "Go elsewhere" headers)))

(defun response-headers-to-string (headers)
  (declare (special out))
  (with-output-to-string (out)
						 (maphash #'(lambda (key value)
									  (format out "~A: ~A~A" key value *CRLF*))
								  headers)))

(defun response-to-string (resp)
  (with-output-to-string (stream)
    (response-to-stream stream resp)))

(defun response-to-stream (stream resp)
  (format stream "HTTP//~A ~A ~A~A" (http-resp-version resp) (http-resp-status-code resp) (http-resp-reason-phrase resp) *CRLF*)
  (format stream "~A~A~A" (response-headers-to-string (http-resp-headers resp)) *CRLF* (http-resp-body resp)))

(defvar *ROUTES* nil)

(defstruct (http-route)
  route-path
  path-match-type
  http-method
  fun-to-call)

(defmacro defroutes (&body &list-of (path path-type method fun))
  `(progn
	 'compile
	 (setq *ROUTES* (list (make-http-route route-path ,(car path) path-match-type ,(car path-type) http-method ,(car method) fun-to-call ,(car fun))))
	 . ,(mapcar #'(lambda (p type m f) `(nconc *ROUTES* (list (make-http-route route-path ,p path-match-type ,type http-method ,m fun-to-call ,f))))
				(cdr path) (cdr path-type) (cdr method) (cdr fun))))

(defun string-starts-with-p (string starts-with)
  (= (or (string-search starts-with string) -1) 0))

(defun route-matches-p (route request)
  (and
   (selectq (http-method route)
			(:all t)
			(:get (string-equal "GET" (http-req-method request)))
			(:post (string-equal "POST" (http-req-method request))))
   (selectq (path-match-type route)
			(:prefix (string-starts-with-p (http-req-uri request) (route-path route)))
			(:exact (string-equal (http-req-uri request) (route-path route))))))

(defun decide-on-route (request)
  (loop for route in *ROUTES*
		when (route-matches-p route request) return route))

(defun http-server (&aux conn)
  (setq conn (chaos:listen "HTTP"))
  (chaos:accept conn)
  (unwind-protect
	  (with-open-stream (stream (chaos:stream conn))
						(let* ((req (parse-request-stream stream))
							   (route (decide-on-route req)))
						  (if route
							  (response-to-stream stream (funcall (fun-to-call route) req))
							(response-to-stream stream (404-resp)))))
	(chaos:close conn)))

(add-initialization "HTTP" '(process-run-temporary-function "HTTP server" 'http-server) nil 'chaos:server-alist)

(defun debug-request (request)
  (declare (special out))
  (200-resp (string-append "Some system info: "
						   (si:system-version-info)
						   "<br>Request info<br>"						   
						   (http-req-method request)
						   "<br>"
						   (http-req-uri request)
						   "<br>"
						   (with-output-to-string (out)
												  (maphash #'(lambda (key value)
															   (format out "~A: ~A~A" key value "<br>"))
														   (http-req-headers request)))
						   (with-output-to-string (out)
												  (mapcar #'(lambda (p)
															   (format out "~A: ~A<br>" (car p) (cdr p)))
														   (http-req-parameters request)))
						   (http-req-body request)
						   "<form method=POST><input name=testing value=foo><input type=submit><//form>"
						   )))

(defun home-page (request)
  (200-resp (string-append header
						   "<h1>Welcome to the Terran Empire Criminal Tracking List<//h1>"
						   footer)))

(defun web-add-criminal (request
						 &aux id name pic crime reward
						 )
  (setq id (cdr (ass 'string-equal "id" (http-req-parameters request))))  
  (setq name (cdr (ass 'string-equal "name" (http-req-parameters request))))
  (setq pic (cdr (ass 'string-equal "pic" (http-req-parameters request))))
  (setq crime (cdr (ass 'string-equal "crime" (http-req-parameters request))))
  (setq reward (cdr (ass 'string-equal "reward" (http-req-parameters request))))
  (if (not (or name pic crime reward))
	  (404-resp)
	  (or (*catch 'too-many-criminals
				  (or (*catch 'already-id
							  (add-criminal id (make-criminal criminal-name name
															  criminal-pic pic
															  criminal-crimes (list crime)
															  criminal-reward reward))
							  (302-resp "//criminals"))
					  (http-resp-with-body-length "400" "Bad Request" (format nil "Already criminal with id ~A" id))))
		  (http-resp-with-body-length "400" "Bad Request" (format nil "Too many criminals, handle this one yourself.")))))
					  
	   
	 

(defun web-list-criminals (request
						   &aux criminals-output
						   )
  (with-criminals-lock
   (setq criminals-output (mapcar #'(lambda (c)
									  (html-criminal (car c) (cdr c)))
								  criminals)))
  (200-resp (format nil "~A <h2>Wanted Criminals<//h2><ul>~{ ~A ~}<//ul><h3>Add a new criminal</h3><form action='//add-criminal'>id: <input type='text' name='id'><br>name: <input type='text' name='name'><br>pic: <input type='text' name='pic'><br>reward: <input type='text' name='reward'><br>crime: <input type='text' name='crime'><input type='submit' value='Charge Criminal'><//form>~A"
					header
					criminals-output
					footer)))


(defun web-remove-criminal (request
							&aux criminal-id)
  (declare (special out))  
  (setq criminal-id (cdr (ass 'string-equal "id" (http-req-parameters request))))
  (if (not criminal-id)
	  (404-resp)
	  (or (*catch 'no-id
				  (remove-criminal (intern criminal-id))
				  (302-resp "//criminals"))
		  (404-resp))))

(defun check-admins (&rest args)
  
  )

(defun web-admin (request
				  &aux admins)
  (setq admins (check-admins (http-req-headers request)))
  )


(defroutes
  ("//debug" ':prefix ':all #'debug-request)
  ("//criminals" ':exact ':get #'web-list-criminals)
  ("//add-criminal" ':prefix ':all #'web-add-criminal)
  ("//remove-criminal" ':prefix ':post #'web-remove-criminal)
  ((string-append "//" (string 10)) ':exact ':all #'web-admin)
  ("//" ':exact ':all #'web-list-criminals))

