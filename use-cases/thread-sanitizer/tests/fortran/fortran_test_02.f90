program race_array
    use omp_lib
    implicit none

    integer, parameter :: n = 100000
    integer, parameter :: m = 10
    integer :: i, tid
    integer :: counts(m)

    counts = 0

    !$omp parallel private(i, tid) shared(counts)
    tid = omp_get_thread_num()

    !$omp do
    do i = 1, n
        ! Each thread randomly increments one of the bins
        counts(mod(i + tid, m) + 1) = counts(mod(i + tid, m) + 1) + 1
    end do
    !$omp end do

    !$omp end parallel

    print *, "Final counts array:"
    print *, counts
    print *, "Expected sum =", n
    print *, "Actual sum   =", sum(counts)

end program race_array
