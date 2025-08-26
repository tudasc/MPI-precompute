program race_example
    use omp_lib
    implicit none

    integer :: i, sum, n

    n = 100000
    sum = 0

    !$omp parallel do shared(sum) private(i)
    do i = 1, n
        sum = sum + 1
    end do
    !$omp end parallel do

    print *, "Final sum = ", sum
    print *, "Expected = ", n
end program race_example
