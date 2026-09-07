(in-package #:ext)

(defun flame-profile-annotation (start-universal-time elapsed-seconds)
  "One-line summary of when a flame profile started and how much wall-clock
time it covered.  Used both as the SVG subtitle (rendered under the title)
and as the NOTES comment in the SVG file header."
  (multiple-value-bind (sec min hour date month year)
      (decode-universal-time start-universal-time)
    (format nil "started ~d-~2,'0d-~2,'0d ~2,'0d:~2,'0d:~2,'0d | elapsed ~,2f s"
            year month date hour min sec elapsed-seconds)))

(defmacro with-flame-profile ((&key (path (format nil "~~/public_html/flame-~d.svg"
                                                  (core:getpid)))
                                 (rate 97) (title "")
                                 (buffer-bytes 0)) &body body)
  "Profile BODY with the sampling profiler and write a flame graph SVG to PATH.

Example:
  (ext:with-flame-profile (:path \"/tmp/my-profile.svg\" :rate 197)
    (my-expensive-computation))

RATE is the sampling frequency in Hz (default 97, a prime to avoid
aliasing with periodic work). TITLE is an optional string for the SVG header.

The SVG records when the profile started and how many seconds of wall-clock
time it covered — as a subtitle under the title, and as a NOTES comment in
the file header.  The measured window is profile-start to profile-stop, so
it excludes symbolication and SVG rendering.

Returns the values of BODY. Signals an error if the profiler is already
running. The profiler is guaranteed to be stopped and reset on any exit
(normal return, throw, or condition)."
  (let ((path-var (gensym "PATH"))
        (rate-var (gensym "RATE"))
        (buffer-bytes-var (gensym "BUFFER-BYTES"))
        (title-var (gensym "TITLE"))
        (start-ut-var (gensym "START-UT"))
        (start-real-var (gensym "START-REAL"))
        (annotation-var (gensym "ANNOTATION"))
        (vals-var (gensym "VALS")))
    `(let ((,path-var ,path)
           (,rate-var ,rate)
           (,buffer-bytes-var ,buffer-bytes)
           (,title-var ,title))
       (when (ext:profile-running-p)
         (error "Sampling profiler is already running"))
       ;; Stamp the wall clock immediately before the profiler starts, so the
       ;; recorded window matches what the samples actually cover.
       (let ((,start-ut-var (get-universal-time))
             (,start-real-var (get-internal-real-time))
             (,annotation-var "")
             ,vals-var)
         (unless (ext:profile-start :rate ,rate-var :buffer-bytes ,buffer-bytes-var)
           (error "Failed to start sampling profiler"))
         (unwind-protect
              (setf ,vals-var (multiple-value-list (progn ,@body)))
           (ext:profile-stop)
           ;; Close the window here, before the (potentially slow)
           ;; symbolication and rendering below.
           (setf ,annotation-var
                 (flame-profile-annotation
                  ,start-ut-var
                  (/ (float (- (get-internal-real-time) ,start-real-var) 1d0)
                     internal-time-units-per-second)))
           (let ((used     (ext:profile-bytes-used))
                 (avail    (ext:profile-bytes-available))
                 (recorded (ext:profile-samples-recorded))
                 (dropped  (ext:profile-samples-dropped)))
             (let ((samples (ext:profile-symbolicated-samples)))
               (when samples
                 (with-open-file (out ,path-var
                                      :direction :output
                                      :if-exists :supersede
                                      :if-does-not-exist :create)
                   (flamegraph:flamegraph :data samples
                                          :output out
                                          :title (if (string= ,title-var "")
                                                     (format nil "clasp ~A" (core:getpid))
                                                     ,title-var)
                                          :subtitle ,annotation-var
                                          :notes ,annotation-var))))
             (ext:profile-reset)
             (format t "Profiling buffer: ~:d / ~:d bytes used (~,1f%), ~:d samples~@[, ~:d   DROPPED (buffer full)~]~%"
                     used avail
                     (if (plusp avail) (/ (* 100.0 used) avail) 0.0)
                     recorded
                     (when (plusp dropped) dropped)))
           )
         (format t "Wrote flame graph to ~s~%" ,path-var)
         (values-list ,vals-var)))))

(defmacro with-allocation-profile
    ((&key
       (path (format nil "~~/public_html/allocation-~d.svg"
                     (core:getpid)))
       (bytes-per-sample (* 1024 1024))
       (max-depth 4096)
       (title "")
       (buffer-bytes 0))
     &body body)
  "Profile managed allocations performed by BODY and write an SVG flame graph.

Example:
  (ext:with-allocation-profile
      (:path \"/tmp/allocation.svg\"
       :bytes-per-sample (* 1024 1024))
    (my-expensive-computation))

BYTES-PER-SAMPLE controls the allocation sampling interval. Values below
1 MiB are clamped to 1 MiB. MAX-DEPTH controls the native stack depth and
is clamped to [1,4096]. BUFFER-BYTES zero selects the 64 MiB default ring.

The flame graph is weighted by attributed bytes rather than record count,
and each stack ends in an allocation-type frame. The measured window
excludes symbolication and SVG rendering.

Returns no values. Signals an error if allocation profiling is already
active. The profiler is stopped and reset during every exit, including
nonlocal exits."
  (let ((path-var (gensym "PATH"))
        (bytes-per-sample-var (gensym "BYTES-PER-SAMPLE"))
        (max-depth-var (gensym "MAX-DEPTH"))
        (buffer-bytes-var (gensym "BUFFER-BYTES"))
        (title-var (gensym "TITLE"))
        (start-ut-var (gensym "START-UT"))
        (start-real-var (gensym "START-REAL"))
        (annotation-var (gensym "ANNOTATION"))
        (wrote-var (gensym "WROTE")))
    `(let ((,path-var ,path)
           (,bytes-per-sample-var ,bytes-per-sample)
           (,max-depth-var ,max-depth)
           (,buffer-bytes-var ,buffer-bytes)
           (,title-var ,title))
       (when (ext:allocation-profile-running-p)
         (error "Allocation profiler is already running"))
       (let ((,start-ut-var (get-universal-time))
             (,start-real-var (get-internal-real-time))
             (,annotation-var "")
             (,wrote-var nil))
         (unless
             (ext:allocation-profile-start
              :bytes-per-sample ,bytes-per-sample-var
              :max-depth ,max-depth-var
              :buffer-bytes ,buffer-bytes-var)
           (error "Failed to start allocation profiler"))
         (unwind-protect
              (progn ,@body (values))
           (ext:allocation-profile-stop)
           (unwind-protect
                (progn
                  (setf ,annotation-var
                        (flame-profile-annotation
                         ,start-ut-var
                         (/ (float
                             (- (get-internal-real-time)
                                ,start-real-var)
                             1d0)
                            internal-time-units-per-second)))
                  (let ((used
                          (ext:allocation-profile-bytes-used))
                        (available
                          (ext:allocation-profile-bytes-available))
                        (recorded
                          (ext:allocation-profile-samples-recorded))
                        (dropped
                          (ext:allocation-profile-samples-dropped))
                        (attributed
                          (ext:allocation-profile-bytes-attributed))
                        (dropped-bytes
                          (ext:allocation-profile-bytes-dropped)))
                    (let ((samples
                            (ext:allocation-profile-symbolicated-samples)))
                      (if (plusp (length samples))
                          (progn
                            (with-open-file
                                (out ,path-var
                                     :direction :output
                                     :if-exists :supersede
                                     :if-does-not-exist :create)
                              (flamegraph:flamegraph
                               :data samples
                               :output out
                               :title
                               (if (string= ,title-var "")
                                   (format nil
                                           "clasp allocations ~A"
                                           (core:getpid))
                                   ,title-var)
                               :subtitle ,annotation-var
                               :notes ,annotation-var
                               :colors "mem"
                               :name-type "Allocation:"
                               :count-name "bytes"))
                            (setf ,wrote-var t))
                          (format t
                                  "No allocation samples captured; no flame graph written.~%")))
                    (format t
                            "Allocation profiling buffer: ~:d / ~:d bytes used (~,1f%), ~:d records, ~:d attributed bytes~%"
                            used available
                            (if (plusp available)
                                (/ (* 100.0 used) available)
                                0.0)
                            recorded attributed)
                    (when (plusp dropped)
                      (format t
                              "Allocation profiler dropped ~:d records representing ~:d bytes (buffer full).~%"
                              dropped dropped-bytes))))
             (ext:allocation-profile-reset)))
         (when ,wrote-var
           (format t "Wrote allocation flame graph to ~s~%"
                   ,path-var))
         (values)))))
