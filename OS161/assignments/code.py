"""
Banker's Algorithm - Safe State Detection
Computes Need matrix, Available vector, and checks for a safe sequence.
"""

def compute_need(max_matrix, allocation):
    """Need[i][j] = Max[i][j] - Allocation[i][j]"""
    n = len(allocation)
    m = len(allocation[0])
    need = [[max_matrix[i][j] - allocation[i][j] for j in range(m)] for i in range(n)]
    return need


def compute_available(existing, allocation):
    """A[j] = E[j] - sum over i of Allocation[i][j]"""
    m = len(existing)
    n = len(allocation)
    available = []
    for j in range(m):
        col_sum = sum(allocation[i][j] for i in range(n))
        available.append(existing[j] - col_sum)
    return available


def is_safe_state(allocation, need, available):
    """
    Runs the safety algorithm.
    Returns (is_safe: bool, safe_sequence: list[int])
    """
    n = len(allocation)          # number of processes
    m = len(available)           # number of resource types

    work = available.copy()
    finish = [False] * n
    safe_sequence = []

    while len(safe_sequence) < n:
        found_process = False

        for i in range(n):
            if finish[i]:
                continue

            # Can process i's remaining need be satisfied by current Work?
            if all(need[i][j] <= work[j] for j in range(m)):
                # Simulate process i running to completion and releasing resources
                for j in range(m):
                    work[j] += allocation[i][j]
                finish[i] = True
                safe_sequence.append(i)
                found_process = True

        if not found_process:
            # No process could be satisfied this round -> stuck -> unsafe
            break

    is_safe = all(finish)
    return is_safe, safe_sequence


def print_matrix(name, matrix, headers):
    print(f"\n{name}:")
    print("       " + "  ".join(f"{h:>4}" for h in headers))
    for i, row in enumerate(matrix):
        print(f"  P{i}  " + "  ".join(f"{v:>4}" for v in row))


def run_bankers_algorithm(existing, allocation, max_matrix=None, need_matrix=None):
    """
    Provide EITHER max_matrix OR need_matrix (not both).
    Prints all required outputs and returns the result.
    """
    if need_matrix is None:
        if max_matrix is None:
            raise ValueError("Must provide either max_matrix or need_matrix")
        need_matrix = compute_need(max_matrix, allocation)

    available = compute_available(existing, allocation)

    m = len(existing)
    headers = [f"R{j}" for j in range(m)]

    print_matrix("Allocation Matrix (P)", allocation, headers)
    print_matrix("Need Matrix", need_matrix, headers)
    print("\nAvailable Vector (A):", available)

    is_safe, sequence = is_safe_state(allocation, need_matrix, available)

    print("\nSystem is SAFE?", "YES" if is_safe else "NO")
    if is_safe:
        seq_str = " -> ".join(f"P{i}" for i in sequence)
        print("Safe Sequence:", f"< {seq_str} >")
    else:
        print("No safe sequence exists. Requesting further resources may deadlock the system.")

    return is_safe, sequence, need_matrix, available


# ---------------------------------------------------------
# Worked example from the lab manual (sanity check)
# ---------------------------------------------------------
if __name__ == "__main__":
    E = [9, 5, 7]  # Existing resources: R0, R1, R2

    P = [  # Allocation matrix
        [0, 1, 0],  # P0
        [2, 0, 0],  # P1
        [3, 0, 2],  # P2
        [2, 1, 1],  # P3
    ]

    Max = [  # Maximum demand matrix
        [7, 5, 3],  # P0
        [3, 2, 2],  # P1
        [9, 0, 2],  # P2
        [2, 2, 2],  # P3
    ]

    run_bankers_algorithm(E, P, max_matrix=Max)
