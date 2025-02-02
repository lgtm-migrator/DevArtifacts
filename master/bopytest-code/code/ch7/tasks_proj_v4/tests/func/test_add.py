import pytest
import tasks
from tasks import Task


def test_add_returns_valid_id(tasks_db):
    # GIVEN an initialized tasks db
    # WHEN a new task is added
    new_task = Task('do something')
    task_id = tasks.add(new_task)
    # THEN returned task_id is of type int
    assert isinstance(task_id, int)

def test_added_task_has_id_set(tasks_db):
    # GIVEN an initialized tasks db
    #   AND a new task is added
    new_task = Task('sit in chair', owner='me', done=True)
    task_id = tasks.add(new_task)
    # WHEN task is retrieved
    task_from_db = tasks.get(task_id)
    # THEN task_id matches id field
    assert task_from_db.id == task_id
    # AND contents are equivalent (except for id)
    # the [:-1] syntax returns a list with all but the last element
    assert task_from_db[:-1] == new_task[:-1]

def test_add_increases_count(db_with_3_tasks,  tasks_db):
    # GIVEN a db with 3 tasks
    #  WHEN another task is added
    tasks.add(Task('throw a party'))
    #  THEN the count increases by 1
    assert tasks.count() == 4




